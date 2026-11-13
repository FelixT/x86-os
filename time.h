#ifndef TIME_H
#define TIME_H

#include "stdint.h"

typedef struct {
   uint8_t seconds;
   uint8_t minutes;
   uint8_t hours;
   uint8_t day; // day of month
   uint8_t month;
   uint8_t year;
   uint8_t weekday; // sunday = 1
} time_t;

time_t get_time();
uint32_t get_seconds();

#endif