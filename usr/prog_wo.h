#include "../windowobj.h"

static inline windowobj_t *register_windowobj(int type, int x, int y, int width, int height) {
   uint32_t addr;
    asm volatile (
      "int $0x30;movl %%ebx, %0;"
      : "=r" (addr)
      : "a" (31)
   );
   windowobj_t *wo = (windowobj_t*)addr;
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
