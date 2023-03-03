#ifndef TERMINAL_H
#define TERMINAL_H

#include "string.h"

void terminal_clear();
void terminal_prompt();
void terminal_keypress(char key);
void terminal_return();
void terminal_clear(void);
void terminal_write(char* str);
void terminal_writeat(char* str, int at);
void terminal_writenumat(int num, int at);
void terminal_backspace(void);


#endif