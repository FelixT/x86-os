#include "time.h"

#include "io.h"

#define BCD2BIN(v) (((v) >> 4) * 10 + ((v) & 0xF))

uint8_t cmos_read(uint8_t reg) {
   outb(0x70, 0x80 | reg);
   return inb(0x71);
}

time_t get_time() {
   while(cmos_read(0x0A) & 0x80); // wait for update in progress
   time_t time;
   time.seconds = BCD2BIN(cmos_read(0x00));
   time.minutes = BCD2BIN(cmos_read(0x02));
   time.hours = BCD2BIN(cmos_read(0x04));
   time.day = BCD2BIN(cmos_read(0x07));
   time.month = BCD2BIN(cmos_read(0x08));
   time.year = BCD2BIN(cmos_read(0x09));
   time.weekday = BCD2BIN(cmos_read(0x06));
   return time;
}

uint32_t get_seconds() {
   time_t time = get_time();
   return time.hours * 3600 + time.minutes * 60 + time.seconds;
}
