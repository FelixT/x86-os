#include "prog.h"

#include "../lib/string.h"
#include "lib/stdio.h"
#include "lib/wo_api.h"

typedef struct interp_state_t {
   windowobj_t *windowobjs[50];
   int wo_count;


} interp_state_t;

interp_state_t state;

// commands:
// print string
// wo <txt/input/btn> x y text

void cmd_print(char *arg) {
   if(!arg) return;
   printf("%s\n", arg);
}

void cmd_wo(char *arg) {
   if(!arg) return;

   char type[8];
   char x[4];
   char y[4];
   char text[20];
   if(!strsplit(type, arg, arg, ' ')) return;
   if(!strsplit(x, arg, arg, ' ')) return;
   if(!strsplit(y, text, arg, ' ')) return;
   
   if(strequ(type, "btn"))
      create_button(NULL, stoi(x), stoi(y), text);

   if(strequ(type, "input"))
      create_text(NULL, stoi(x), stoi(y), text);

   if(strequ(type, "txt"))
      create_text_static(NULL, stoi(x), stoi(y), text);
}

void parse_line(char *line) {
   char command[10];
   char arg[50];
   strsplit((char*)command, (char*)arg, line, ' ');
   strtoupper(command, command);

   if(strequ(command, "PRINT"))
      cmd_print(arg);
   else if(strequ(command, "WO"))
      cmd_wo(arg);
   else
      printf("<unrecognised>\n");
}

void _start() {
   printf("f3BASIC interpreter\n");
   override_term_checkcmd((uint32_t)NULL);
   
   char *buf = (char*)sbrk(100);

   while(true) {
      read(0, buf, 100);

      parse_line(buf);
   }

   exit(0);
}