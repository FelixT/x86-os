#ifndef STDIO_H
#define STDIO_H

#include <stdint.h>
#include <stddef.h>
#include <stdarg.h>

#define STDIO_BUFFER_SIZE 2048*8

typedef struct {
   uint8_t *buffer; // used for buffered writes
   uint32_t buffer_pos;
   char mode[4];
   int is_open;
   int is_stream;
   int fd;
} FILE;

#define SEEK_SET 0
#define SEEK_CUR 1
#define SEEK_END 2

FILE* fopen(const char *filename, const char *mode);
size_t fwrite(const void *ptr, size_t size, size_t count, FILE *stream);
size_t fread(void *ptr, size_t size, size_t count, FILE *stream);
int fclose(FILE *stream);
void fclose_all();
int fflush(FILE *stream);
void debug_println(const char *format, ...);
void printf(const char *format, ...);
void printf_w(const char *format, int window, ...);
int fileno(FILE *stream);
int fseek(FILE *stream, int pos, int type);
void vfprintf(FILE *stream, const char *format, va_list args);
void fprintf(FILE *stream, const char *format, ...);
int ftell(FILE *stream);

#endif