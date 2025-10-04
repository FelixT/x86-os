#ifndef STRING_H
#define STRING_H

#include <stdbool.h>
#include <stdint.h>
#include <stdarg.h>
#include <stddef.h>

void strtoupper(char* dest, char* src);
void strcpy(char* dest, char* src);
void strncpy(char* dest, const char* src, size_t size);
void strcpy_fixed(char* dest, char* src, int length);
int stoi(char* str);
bool strsplit(char* dest1, char* dest2, char* src, char splitat);
bool strstartswith(char* src, char* startswith);
bool strsplit_last(char* dest1, char* dest2, char* src, char splitat);
int strlen(char* str);
bool strequ(char* str1, char* str2);
void uinttohexstr(uint32_t num, char* out);
void uinttostr(uint32_t num, char* out);
void inttostr(int num, char* out);
void vsnprintf(char *buffer, size_t size, char *format, va_list args);
void sprintf(char *buffer, char *format, ...);
void snprintf(char *buffer, size_t size, char *format, ...);
char *strchr(const char *str, int c);
char *strcat(char *dest, const char *src);
uint32_t hextouint(char *str);
int strtoint(char *str);
void memset(void *dest, uint8_t ch, int count);

#endif