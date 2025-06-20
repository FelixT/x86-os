#ifndef FILE_H
#define FILE_H

#include <stdint.h>
#include <stddef.h>

typedef struct {
   char *path;
   uint8_t *buffer;
   uint32_t size;
   uint32_t content_size;
   uint32_t position;
   char mode[4];
   int is_open;
   int dirty; // Track if buffer has been modified
} FILE;

#define MAX_FILES 16

FILE* fopen(const char* filename, const char* mode);
size_t fwrite(const void* ptr, size_t size, size_t count, FILE* stream);
size_t fread(void* ptr, size_t size, size_t count, FILE* stream);
int fclose(FILE* stream);
int fflush(FILE* stream);

#endif