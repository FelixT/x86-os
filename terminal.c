#include <stdint.h>
#include <stddef.h>
#include <stdbool.h>

#include "terminal.h"

size_t terminal_index;

extern int videomode;

#define command_maxlen 40
char command_buffer[command_maxlen+1];
int command_index = 0;

uint16_t colour(uint8_t fg, uint8_t bg) {
   return fg | bg << 4;
}

uint16_t entry(char c, uint8_t colour) {
   return (uint16_t) c | (uint16_t) colour << 8;
}

static inline void outb(uint16_t port, uint8_t val) {
   asm volatile("outb %0, %1" : : "a"(val), "Nd"(port));
}

void terminal_setcursor(int offset) {
   terminal_index = offset%(80*25);
   outb(0x3D4, 0x0F);
   outb(0x3D5, (uint8_t) (terminal_index & 0xFF));
   outb(0x3D4, 0x0E);
   outb(0x3D5, (uint8_t) ((terminal_index >> 8) & 0xFF));
}

void terminal_scroll(void) {
   uint16_t *terminal_buffer = (uint16_t*) 0xB8000;

   for(int i = 80; i < 80*25; i++)
      terminal_buffer[i-80] = terminal_buffer[i];

   for(int i = 80*24; i < 80*25; i++)
      terminal_buffer[i] = entry(' ', colour(15, 0));
}

void terminal_write(char* str) {
   uint16_t *terminal_buffer = (uint16_t*) 0xB8000;
   int i = 0;
   while(str[i] != '\0') {
      if(str[i] == '\n') {
         if(terminal_index >= 80*24) {
            terminal_scroll();
            terminal_setcursor(80*24);
         } else {
            terminal_setcursor(terminal_index + (80-(terminal_index%80)));
         }
         i++;
      } else {
         terminal_buffer[terminal_index] = entry(str[i++], colour(15, 0));

         terminal_setcursor(terminal_index+1);
      }
   }
}

void terminal_clear(void) {
   videomode = 0;
   uint16_t *terminal_buffer = (uint16_t*) 0xB8000;
   for(unsigned i = 0; i < 80*25; i++) {
      terminal_buffer[i] = entry(' ', colour(15, 0));
   }

   terminal_setcursor(0);
   return;
}

void terminal_prompt(void) {
   terminal_write("\n>");
}

void terminal_writeat(char* str, int at) {
   uint16_t *terminal_buffer = (uint16_t*) 0xB8000;
   int i = 0;
   while(str[i] != '\0') {
      terminal_buffer[at++] = entry(str[i++], colour(15, 0));
   }
}

void terminal_backspace(void) {
   if(command_index > 0) {
      command_index--;
      uint16_t *terminal_buffer = (uint16_t*) 0xB8000;
      terminal_index-=1;
      terminal_buffer[terminal_index] = entry(' ', colour(15, 0));
      terminal_setcursor(terminal_index);
   }
}

void terminal_writenumat(int num, int at) {
   char out[20]; // allocate more memory than required for int's maxvalue
   
   inttostr(num, out);

   terminal_writeat(out, at);
}

void terminal_writenum(int num) {
   // get number length in digits
   int tmp = num;
   int length = 0;
   while(tmp > 0) {
      length++;
      tmp/=10;
   }

   terminal_writenumat(num, terminal_index);
   terminal_setcursor(terminal_index+length);
}

void terminal_keypress(char key) {
   if(command_index < command_maxlen) {
      char letter[2] = "x";
      letter[0] = key;
      terminal_write(letter);

      if(letter[0] != '\0') {
         command_buffer[command_index++] = letter[0];
      }
   } else {} // no more space in buffer
}

void terminal_checkcmd(char* command) {
   if(strcmp(command, "WICKED")) {
      terminal_write("\nyep, wicked\n");
   }

   if(strcmp(command, "CLEAR")) {
      terminal_clear();
   }
}

void terminal_return() {
   terminal_write("\n");

   command_buffer[command_index] = '\0';
   terminal_write(command_buffer);
   terminal_checkcmd(command_buffer);

   command_index = 0;

   terminal_prompt();
}