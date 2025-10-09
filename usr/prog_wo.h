#include "../windowobj.h"

static inline windowobj_t *register_windowobj(int type, int x, int y, int width, int height) {
   uint32_t addr;
    asm volatile (
      "int $0x30;movl %%ebx, %0;"
      : "=r" (addr)
      : "a" (31)
   );
   windowobj_t *wo = (windowobj_t*)addr;
   if(!wo) {
      return NULL;
   }
   wo->type = type;
   wo->x = x;
   wo->y = y;
   wo->width = width;
   wo->height = height;
   if(type != WO_BUTTON) {
      wo->texthalign = false;
      wo->textvalign = false;
   }
   return wo;
}

static inline windowobj_t *windowobj_add_child(windowobj_t *parent, int type, int x, int y, int width, int height) {
   uint32_t addr;
    asm volatile (
      "int $0x30;movl %%ebx, %0;"
      : "=r" (addr)
      : "a" (57),
      "b" ((uint32_t)parent)
   );
   windowobj_t *child = (windowobj_t*)addr;
   if(!child) {
      return NULL;
   }
   child->type = type;
   child->x = x;
   child->y = y;
   child->width = width;
   child->height = height;
   if(type != WO_BUTTON) {
      child->texthalign = false;
      child->textvalign = false;
   }
   return child;
}
