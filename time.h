#ifndef TIME_H
#define TIME_H

#include "stdint.h"

typedef struct {
   uint8_t seconds;
   uint8_t minutes;
   uint8_t hours;
} time_t;

time_t get_time();
uint32_t get_seconds();

#endif