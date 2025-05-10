#ifndef STRING_H
#define STRING_H

#include <stdbool.h>
#include <stdint.h>
#include <stdarg.h>

void strtoupper(char* dest, char* src);
void strcpy(char* dest, char* src);
void strcpy_fixed(char* dest, char* src, int length);
int stoi(char* str);
bool strsplit(char* dest1, char* dest2, char* src, char splitat);
bool strstartswith(char* src, char* startswith);
int strlen(char* str);
bool strcmp(char* str1, char* str2);
void uinttohexstr(uint32_t num, char* out);
void uinttostr(uint32_t num, char* out);
void inttostr(int num, char* out);
void vsprintf(char *buffer, char *format, va_list args);
void sprintf(char *buffer, char *format, ...);
char *strchr(const char *str, int c);

#endif