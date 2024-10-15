#ifndef SURFACE_T_H
#define SURFACE_T_H

#include <stdint.h>

typedef struct {
   uint32_t buffer;
   int width;
   int height;
} surface_t;

#endif