#ifndef DIALOGS_H
#define DIALOGS_H

#include "wo_api.h"

typedef struct dialog_t {
   bool active;
   int type;
   int window;
   void (*callback)(char *out);
   // txtinput specific
   windowobj_t *inputtxt;
} dialog_t;

bool dialog_msg(char *title, char *text);
int dialog_input(char *text, void *return_func);

#define MAX_DIALOGS 10

#define DIALOG_MSG 0
#define DIALOG_INPUT 1

#endif
