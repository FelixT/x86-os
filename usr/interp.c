#include "prog.h"

#include "../lib/string.h"
#include "lib/dialogs.h"
#include "lib/stdio.h"
#include "lib/stdlib.h"
#include "lib/ui/ui_mgr.h"
#include "lib/ui/ui_button.h"
#include "lib/ui/ui_input.h"
#include "lib/ui/ui_label.h"
#include "lib/map.h"

#define WO_COUNT 32
#define VAR_NAME_LENGTH 16

typedef struct var_wo_t {
   wo_t *wo;
   char function[VAR_NAME_LENGTH]; // only allows one function for now
} var_wo_t;

typedef struct interp_state_t {
   map_t variables;
   bool in_func;
   int return_location; // todo return stack
   int location;
   char *buffer;
   int buffer_size;
   char *linebuf;
   var_wo_t wos[WO_COUNT];
   int wo_count;
   bool no_exe;
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

void cmd_help() {
   printf("Output text:\n");
   printf("  print str\n");
   printf("Create widgets (window objects):\n");
   printf("  createbtn wo_varname x y text\n");
   printf("  createlabel wo_varname x y text\n");
   printf("  createinput wo_varname x y placeholder\n");
   printf("Update widget parameters:\n");
   printf("  setfunc wo_varname func_varname\n");
   printf("  settext wo_varname text\n");
   printf("Set/get/free variables:\n");
   printf("  set str_varname str\n");
   printf("  seti int_varname int\n");
   printf("  get any_varname / getvars\n");
   printf("  free any_varname\n");
   printf("Create functions:\n");
   printf("  func func_varname / endfunc\n");
   printf("  call func_varname\n");
   printf("Load file/reset system:\n");
   printf("  open str_varname filepath\n");
   printf("  run str_varname\n");
   printf("  clear / reset\n");
}

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
   printf_w("%s\n", dialog->window, arg);
}

void cmd_call(char *arg);
void generic_callback(void *wo) {
   // get var_wo_t
   for(int i = 0; i < state.wo_count; i++) {
      if(state.wos[i].wo == wo) {
         printf("Found wo with function %s\n", state.wos[i].function);
         if(strlen(state.wos[i].function) > 0)
            cmd_call(state.wos[i].function);
         end_subroutine();
      }
   }
   debug_println("Couldn't find wo var for callback");
   end_subroutine();
}

int cmd_createbtn(char *arg) {
   if(!arg) return -1;

   char varname[VAR_NAME_LENGTH];
   char x[4];
   char y[4];
   char text[20];
   if(!strsplit(varname, arg, arg, ' ')
   || !strsplit(x, arg, arg, ' ')
   || !strsplit(y, text, arg, ' ')) {
      printf("Invalid args for createbtn\n");
      return -1;
   }
   
   int index = state.wo_count;
   state.wo_count++;

   if(index == WO_COUNT) {
      printf("No free wovars\n");
      return -1; // no free wovars
   }

   wo_t *button = create_button(stoi(x), stoi(y), strlen(text)*(get_font_info().width+get_font_info().padding) + 10, 20, text);
   state.wos[index].wo = button;
   strcpy(state.wos[index].function, "");
   button->release_func = (void*)&generic_callback;
   ui_add(dialog->ui, button);
   ui_draw(dialog->ui);
   redraw_w(dialog->window);

   var_set(varname, VAR_WO, (void*)index);

   return index;
}

int cmd_createinput(char *arg) {
   if(!arg) return -1;

   char varname[VAR_NAME_LENGTH];
   char x[4];
   char y[4];
   char text[20];
   if(!strsplit(varname, arg, arg, ' ')
   || !strsplit(x, arg, arg, ' ')
   || !strsplit(y, text, arg, ' ')) {
      printf("Invalid args for createinput\n");
      return -1;
   }
   
   int index = state.wo_count;
   state.wo_count++;

   if(index == WO_COUNT) {
      printf("No free wovars\n");
      return -1; // no free wovars
   }

   wo_t *input = create_input(stoi(x), stoi(y), 100, 20);
   set_input_text(input, text);
   get_input(input)->placeholder = true;
   state.wos[index].wo = input;
   ui_add(dialog->ui, input);
   ui_draw(dialog->ui);
   redraw_w(dialog->window);

   var_set(varname, VAR_WO, (void*)index);

   return index;
}

int cmd_createlabel(char *arg) {
   if(!arg) return -1;

   char varname[VAR_NAME_LENGTH];
   char x[4];
   char y[4];
   char text[20];
   if(!strsplit(varname, arg, arg, ' ')
   || !strsplit(x, arg, arg, ' ')
   || !strsplit(y, text, arg, ' ')) {
      printf("Invalid args for createlabel\n");
      return -1;
   }
   
   int index = state.wo_count;
   state.wo_count++;

   if(index == WO_COUNT) {
      printf("No free wovars\n");
      return -1; // no free wovars
   }

   wo_t *txt = create_label(stoi(x), stoi(y), strlen(text)*(get_font_info().width+get_font_info().padding) + 10, 20, text);
   state.wos[index].wo = txt;
   ui_add(dialog->ui, txt);
   ui_draw(dialog->ui);
   redraw_w(dialog->window);

   var_set(varname, VAR_WO, (void*)index);

   return index;
}

void cmd_set(char *arg) {
   char name[8]; // variable name
   char value[128];
   if(!strsplit(name, value, arg, ' ')) return;

   var_set(name, VAR_STR, value);
}

void cmd_seti(char *arg) {
   char name[8]; // variable name
   char value[40];
   if(!strsplit(name, value, arg, ' ')) return;

   var_set(name, VAR_INT, (void*)strtoint(value));
}

// declare the start of a function
void cmd_func(char *arg) {
   func_t *func = malloc(sizeof(func_t));
   func->location = state.location;
   debug_println("Location %i\n", func->location);
   state.no_exe = true;

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

void cmd_free(char *arg) {
   var_t *var = map_lookup(&(state.variables), arg);
   if(!var) {
      printf("Not found\n");
      return;
   }
   switch(var->type) {
      case VAR_INT:
         printf("freeing int %s\n", arg);
         break;
      case VAR_STR:
         printf("freeing str %s\n", arg);
         char *str = var->value;
         free(str);
         break;
      case VAR_WO:
         printf("freeing wo %s\n", arg);
         int wo = (int)var->value;
         if(wo < 0 || wo > state.wo_count) {
            printf("invalid wo %i\n", wo);
            return;
         }
         destroy_wo(state.wos[wo].wo);
         break;
      case VAR_FUNC:
         func_t *func = (func_t*)var->value;
         free(func);
         break;
      default:
         printf("Invalid type %i\n", var->type);
         return;
   }

   free(var);
   map_remove(&(state.variables), arg);
}

void cmd_reset() {
   for(uint32_t i = 0; i < state.variables.size; i++) {
      map_entry_t *entry = &state.variables.table[i];
      if(entry->occupied) {
         cmd_free(entry->key);
      }
   }
   for(int i = 0; i < state.wo_count; i++) {
      free(state.wos[i].wo);
   }
   state.wo_count = 0;
   clear_w(dialog->window);
   redraw_w(dialog->window);
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
   state.no_exe = false;
   if(!state.in_func) return;
   state.location = state.return_location;
   state.in_func = false;
}

void cmd_setfunc(char *arg) {
   // setfunc woname funcname

   char woname[VAR_NAME_LENGTH];
   char funcname[VAR_NAME_LENGTH];
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

void cmd_settext(char *arg) {
   // setfunc woname strname

   char woname[VAR_NAME_LENGTH];
   char strname[VAR_NAME_LENGTH];
   if(!strsplit(woname, strname, arg, ' ')) {
      debug_println("settext: Invalid arguments");
   }

   var_t *wovar = map_lookup(&(state.variables), woname);
   if(!wovar) {
      printf("settext: wo '%s' not found\n", woname);
      return;
   }
   if(wovar->type != VAR_WO) {
      printf("settext: var '%s' isn't a wo\n", woname);
      return;
   }
   var_t *strvar = map_lookup(&(state.variables), strname);
   if(!strvar) {
      printf("settext: var '%s' not found\n", strname);
      return;
   }
   if(strvar->type != VAR_STR) {
      printf("settext: var '%s' isn't a str\n", strname);
      return;
   }

   if((int)wovar->value < 0 || (int)wovar->value > state.wo_count) {
      printf("settext: Invalid wo '%s'/%i", woname, wovar->value);
      return;
   }
   printf("settext: Setting wo '%s' (%i) text to '%s'\n", woname, wovar->value, strvar->value);

   wo_t *wo = state.wos[(int)wovar->value].wo;
   if(wo->type == WO_BUTTON) {
      strncpy(get_button(wo)->label, strvar->value, sizeof(get_button(wo)->label));
   } else if(wo->type == WO_INPUT) {
      strncpy(get_input(wo)->text, strvar->value, sizeof(get_input(wo)->text));
   } else if(wo->type == WO_LABEL) {
      strncpy(get_label(wo)->label, strvar->value, sizeof(get_label(wo)->label));
   }

}

void cmd_open(char *arg) {
   char varname[VAR_NAME_LENGTH];
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
      free(buffer);
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
   state.buffer_size = strlen(state.buffer) + 1;
   char *start = state.buffer + state.location;
   // split at newline
   char tmp[128];
   int i = 0;
   while(state.location < state.buffer_size) {
      start = state.buffer + state.location;
      strsplit(tmp, NULL, start, '\n');
      int len = strlen(tmp);
      if(len==0) {
         // handle blank line
         if(state.location + 1 < state.buffer_size) {
            state.location++;
            i++;
            continue;
         } else {
            break;
         }
      }
      state.location += len + 1;
      parse_line(tmp);
      i++;
   }
}

void cmd_clear() {
   clear_w(dialog->window);
}

void cmd_getvars() {
   for(uint32_t i = 0; i < state.variables.size; i++) {
      map_entry_t *entry = &state.variables.table[i];
      if(entry->occupied) {
         printf("Key: %s\n", entry->key);
         var_t *var = entry->value;
         printf("Type %i\n", var->type);
      }
   }
}

void parse_line(char *line) {
   while(line[0] == ' ')
      line++; // skip leading spaces

   char command[10];
   char arg[50];
   strsplit((char*)command, (char*)arg, line, ' ');
   tolower(command);

   if(state.no_exe && !strequ(command, "endfunc")) {
      printf("no_exe flag set, skipping line\n");
      return;
   }

   if(strequ(command, "print"))
      cmd_print(arg);
   else if(strequ(command, "createbtn"))
      cmd_createbtn(arg);
   else if(strequ(command, "createinput"))
      cmd_createinput(arg);
   else if(strequ(command, "createlabel"))
      cmd_createlabel(arg);
   else if(strequ(command, "set"))
      cmd_set(arg);
   else if(strequ(command, "seti"))
      cmd_seti(arg);
   else if(strequ(command, "get"))
      cmd_get(arg);
   else if(strequ(command, "free"))
      cmd_free(arg);
   else if(strequ(command, "reset"))
      cmd_reset(arg);
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
   else if(strequ(command, "settext"))
      cmd_settext(arg);
   else if(strequ(command, "open"))
      cmd_open(arg);
   else if(strequ(command, "run"))
      cmd_run(arg);
   else if(strequ(command, "help"))
      cmd_help();
   else if(strequ(command, "clear"))
      cmd_clear();
   else if(strequ(command, "getvars"))
      cmd_getvars();
   else if(strequ(command, "//"))
      return; // comment, do nothing
   else
      printf("<unrecognised>\n");
}

void line_callback(char *buffer) {
   strcpy(state.linebuf, buffer);
   strcat(state.buffer, state.linebuf);
   strcat(state.buffer, "\n"); // use linebreak as separator

   state.location += strlen(state.linebuf)+1;
   state.buffer_size = state.location;
   parse_line(state.linebuf);

   end_subroutine();
}

void _start() {
   memset(&state, 0, sizeof(interp_state_t));

   // interp window
   printf("f3BASIC interpreter\n");
   override_term_checkcmd((uint32_t)line_callback);
   set_window_title("f3BASIC");

   // output window
   dialog = get_dialog(get_free_dialog());
   dialog_init(dialog, create_window(340, 240));
   dialog_set_title(dialog, "f3BASIC output");
   set_window_minimised(false, -1);
   
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