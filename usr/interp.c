#include "prog.h"

#include "../lib/string.h"
#include "lib/dialogs.h"
#include "lib/stdio.h"
#include "lib/ui/ui_mgr.h"
#include "lib/ui/ui_button.h"
#include "lib/ui/ui_input.h"
#include "lib/ui/ui_label.h"
#include "lib/map.h"

typedef struct var_wo_t {
   wo_t *wo;
   char function[16]; // only allows one function for now
} var_wo_t;

#define WO_COUNT 32

typedef struct interp_state_t {
   map_t variables;
   bool in_func;
   int return_location; // todo return stack
   int location;
   char *buffer;
   char *linebuf;
   var_wo_t wos[WO_COUNT];
   int wo_count;
} interp_state_t;

#define VAR_INT 0
#define VAR_STR 1
#define VAR_WO 2
#define VAR_FUNC 3

typedef struct var_t {
   void *value;
   int type;
} var_t;

typedef struct func_t {
   int location;
} func_t;

interp_state_t state;
dialog_t *dialog;

// commands:
// print string
// wo <txt/input/btn> x y text
// set varname str, seti varname int, setwo varname <txt/input/btn> x y text
// get varname,
// func varname, endfunc
// call funcname
// setfunc woname funcname
// open varname filepath
// run strname
// buffer startindex

void var_set(char *name, int type, void *value) {
   var_t *var = malloc(sizeof(var_t));
   var->type = type;
   if(var->type == VAR_STR) {
      var->value = malloc(strlen(value) + 1);
      strncpy(var->value, value, strlen(value) + 1);
   } else {
      // store raw value
      var->value = value;
   }
   map_insert(&(state.variables), name, var);
}

void cmd_print(char *arg) {
   if(!arg) return;
   printf("%s\n", arg);
}

void cmd_call(char *arg);
void generic_callback(void *wo) {
   // get var_wo_t
   for(int i = 0; i < state.wo_count; i++) {
      if(state.wos[i].wo == wo) {
         printf("Found wo with function %s\n", state.wos[i].function);
         cmd_call(state.wos[i].function);
         end_subroutine();
      }
   }
   debug_println("Couldn't find wo var for callback");
   end_subroutine();
}

int cmd_wo(char *arg) {
   if(!arg) return -1;

   char type[8];
   char x[4];
   char y[4];
   char text[20];
   if(!strsplit(type, arg, arg, ' ')) return -1;
   if(!strsplit(x, arg, arg, ' ')) return -1;
   if(!strsplit(y, text, arg, ' ')) return -1;
   
   int index = state.wo_count;
   state.wo_count++;

   if(index == WO_COUNT) {
      return -1; // no free wovars
   }

   if(strequ(type, "btn")) {
      wo_t *button = create_button(stoi(x), stoi(y), strlen(text)*(get_font_info().width+get_font_info().padding) + 10, 20, text);
      state.wos[index].wo = button;
      strcpy(state.wos[index].function, "");

      debug_println("wo: Created button %i", index);
      button->release_func = (void*)&generic_callback;
      ui_add(dialog->ui, button);
      ui_draw(dialog->ui);
   }

   if(strequ(type, "input")) {
      wo_t *input = create_input(stoi(x), stoi(y), 100, 20);
      set_input_text(input, text);
      state.wos[index].wo = input;
      ui_add(dialog->ui, input);
      ui_draw(dialog->ui);
   }

   if(strequ(type, "txt")) {
      wo_t *txt = create_label(stoi(x), stoi(y), strlen(text)*(get_font_info().width+get_font_info().padding) + 10, 20, text);
      state.wos[index].wo = txt;
      ui_add(dialog->ui, txt);
      ui_draw(dialog->ui);
   }

   return index;
}

void cmd_set(char *arg) {
   char name[8]; // variable name
   char value[40];
   if(!strsplit(name, value, arg, ' ')) return;

   var_set(name, VAR_STR, value);
}

void cmd_seti(char *arg) {
   char name[8]; // variable name
   char value[40];
   if(!strsplit(name, value, arg, ' ')) return;

   var_set(name, VAR_INT, (void*)strtoint(value));
}

void cmd_setwo(char *arg) {
   char name[8]; // variable name
   char args[40]; // arguments for 'wo' command
   if(!strsplit(name, args, arg, ' ')) return;

   int woindex = cmd_wo(args);
   if(woindex < 0) {
      printf("setwo: Couldn't create wo");
      return;
   }

   var_set(name, VAR_WO, (void*)woindex);
}

// declare the start of a function
void cmd_func(char *arg) {
   func_t *func = malloc(sizeof(func_t));
   func->location = state.location;
   debug_println("Location %i\n", func->location);

   var_set(arg, VAR_FUNC, func);
}

void cmd_get(char *arg) {
   var_t *var = map_lookup(&(state.variables), arg);
   if(!var) {
      printf("Not found\n");
      return;
   }
   if(var->type == VAR_INT)
      printf("get: Found int %i\n", var->value);
   if(var->type == VAR_STR)
      printf("get: Found str %s\n", var->value);
   if(var->type == VAR_WO)
      printf("get: Found wo %i\n", var->value);
   if(var->type == VAR_FUNC)
      printf("get: Found func location %i\n", ((func_t*)var->value)->location);
}

void cmd_buffer(char *arg) {
   int offset = arg ? strtoint(arg) : 0;
   printf("%s\n", state.buffer + offset);
}

void parse_line(char *line);

void cmd_call(char *arg) {
   var_t *var = NULL;
   if(arg) {
      var = map_lookup(&(state.variables), arg);
      if(!var) {
         printf("call: var '%s' not found\n", arg);
         return;
      }
      if(var->type != VAR_FUNC) {
         printf("call: var isn't a func\n", var->type);
         return;
      }
   }

   if(state.in_func) {
      printf("call: Already in function\n");
      return;
   }
   
   state.return_location = state.location;
   printf("call: Return location %i\n", state.return_location);

   state.location = arg ? ((func_t*)var->value)->location : 0;
   state.in_func = true;
   char *start = state.buffer + state.location;
   char tmp[128];
   // split at newline
   while(true) {
      start = state.buffer + state.location;
      strsplit(tmp, NULL, start, '\n');
      if(strlen(tmp)==0) break;
      state.location += strlen(tmp) + 1;
      debug_println("Line %s", tmp);
      parse_line(tmp);
   }
}

void cmd_endfunc(char *arg) {
   (void)arg;
   if(!state.in_func) return;
   state.location = state.return_location;
   state.in_func = false;
}

void cmd_setfunc(char *arg) {
   // setfunc woname funcname

   char woname[16];
   char funcname[16];
   if(!strsplit(woname, funcname, arg, ' ')) {
      debug_println("setfunc: Invalid arguments");
   }

   var_t *wovar = map_lookup(&(state.variables), woname);
   if(!wovar) {
      printf("setfunc: wo '%s' not found\n", woname);
      return;
   }
   if(wovar->type != VAR_WO) {
      printf("setfunc: var '%s' isn't a wo\n", woname);
      return;
   }
   var_t *funcvar = map_lookup(&(state.variables), funcname);
   if(!funcvar) {
      printf("setfunc: func '%s' not found\n", funcname);
      return;
   }
   if(funcvar->type != VAR_FUNC) {
      printf("setfunc: var '%s' isn't a func\n", funcname);
      return;
   }

   if((int)wovar->value < 0 || (int)wovar->value > state.wo_count) {
      printf("setfunc: Invalid wo '%s'/%i", woname, wovar->value);
      return;
   }
   printf("setfunc: Setting wo '%s' (%i) func to '%s'\n", woname, wovar->value, funcname);

   strcpy(state.wos[(int)wovar->value].function, funcname);

}

void cmd_open(char *arg) {
   char varname[16];
   char path[40];
   if(!strsplit(varname, path, arg, ' ')) {
      debug_println("open: Invalid arguments");
   }

   FILE *f = fopen(path, "r");
   if(!f) {
      printf("open: Couldn't open file '%s'\n", path);
   }

   int size = fsize(fileno(f));
   char *buffer = malloc(size+1);
   if(!fread(buffer, size, 1, f)) {
      free(buffer, size+1);
      printf("open: Couldn't read file '%s'\n", path);
      fclose(f);
      return;
   }
   fclose(f);
   buffer[size] = '\0';
   var_set(varname, VAR_STR, buffer);
}

void cmd_run(char *arg) {
   var_t *var = map_lookup(&(state.variables), arg);
   if(!var) {
      printf("Not found\n");
      return;
   }
   if(var->type != VAR_STR) {
      printf("run: Not a string");
      return;
   }
   strcpy(state.buffer, var->value);
   state.location = 0;
   char *start = state.buffer + state.location;
   // split at newline
   char tmp[128];
   int i = 0;
   while(true) {
      start = state.buffer + state.location;
      strsplit(tmp, NULL, start, '\n');
      if(strlen(tmp)==0) break;
      state.location += strlen(tmp) + 1;
      //printf("%i: %s\n", i, tmp);
      parse_line(tmp);
      i++;
   }
}

void parse_line(char *line) {
   while(line[0] == ' ')
      line++; // skip leading spaces

   char command[10];
   char arg[50];
   strsplit((char*)command, (char*)arg, line, ' ');
   tolower(command);

   if(strequ(command, "print"))
      cmd_print(arg);
   else if(strequ(command, "wo"))
      cmd_wo(arg);
   else if(strequ(command, "set"))
      cmd_set(arg);
   else if(strequ(command, "seti"))
      cmd_seti(arg);
   else if(strequ(command, "setwo"))
      cmd_setwo(arg);
   else if(strequ(command, "get"))
      cmd_get(arg);
   else if(strequ(command, "func"))
      cmd_func(arg);
   else if(strequ(command, "buffer"))
      cmd_buffer(arg);
   else if(strequ(command, "call"))
      cmd_call(arg);
   else if(strequ(command, "endfunc"))
      cmd_endfunc(arg);
   else if(strequ(command, "setfunc"))
      cmd_setfunc(arg);
   else if(strequ(command, "open"))
      cmd_open(arg);
   else if(strequ(command, "run"))
      cmd_run(arg);
   else
      printf("<unrecognised>\n");
}

void line_callback(char *buffer) {
   strcpy(state.linebuf, buffer);
   strcat(state.buffer, state.linebuf);
   strcat(state.buffer, "\n"); // use linebreak as separator

   state.location += strlen(state.linebuf)+1;
   parse_line(state.linebuf);

   end_subroutine();
}

void _start() {

   // interp window
   printf("f3BASIC interpreter\n");
   override_term_checkcmd((uint32_t)line_callback);

   // output window
   dialog = get_dialog(get_free_dialog());
   dialog_init(dialog, create_window(340, 240));
   dialog_set_title(dialog, "f3BASIC output");
   
   state.wo_count = 0;
   state.in_func = false;
   map_init(&(state.variables), 128); // max 128 vars

   state.buffer = (char*)sbrk(0x4000); // overall buffer
   state.buffer[0] = '\0';

   state.linebuf = (char*)sbrk(128);

   while(true) {
      yield();
   }

   /*windowobj_t *pos = create_text_static(NULL, 150, 0, "");
   pos->width = 100;

   while(true) {
      char buf[10];
      inttostr(state.location, buf);
      set_text(pos, buf);
      
      read(0, state.linebuf, 128);
      strcat(state.buffer, state.linebuf);
      strcat(state.buffer, "\n"); // use linebreak as separator

      state.location += strlen(state.linebuf)+1;
      parse_line(state.linebuf);

      inttostr(state.location, buf);
      set_text(pos, buf);
   }*/

   exit(0);
}