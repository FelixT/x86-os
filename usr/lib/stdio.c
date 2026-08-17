// libc style file io functions

#include "stdio.h"
#include "stdlib.h"
#include "../prog.h"
#include "../../lib/string.h"
#include <stddef.h>

#define MAX_FILES 16
static FILE file_table[MAX_FILES];
static int files_initialized = 0;

void init_files(void) {
    if(!files_initialized) {
        memset(file_table, 0, sizeof(file_table));
        files_initialized = 1;
    }
}

FILE *get_free_file(void) {
    init_files();
    for(int i = 0; i < MAX_FILES; i++) {
        if (!file_table[i].is_open) {
            return &file_table[i];
        }
    }
    return NULL;
}

FILE *fopen(const char *filename, const char *mode) {
    FILE *file = get_free_file();
    if(!file) return NULL;
    
    // copy filename
    file->path = (char*)malloc(512);
    if(!file->path) return NULL;
    strcpy(file->path, (char*)filename);
    
    // copy mode
    strncpy(file->mode, mode, sizeof(file->mode) - 1);
    file->mode[sizeof(file->mode) - 1] = '\0';
    
    bool mode_r = strchr(mode, 'r') != NULL;
    bool mode_w = strchr(mode, 'w') != NULL;
    bool mode_a = strchr(mode, 'a') != NULL;
    bool mode_plus = strchr(mode, '+') != NULL;

    if(!mode_r && !mode_w && !mode_a) {
        debug_write_str("fopen: invalid mode\n");
        free(file->path);
        file->path = NULL;
        return NULL;
    }

    int flag = 0;
    if(mode_w || mode_a)
        flag |= FS_FLAG_CREATE;
    if(mode_w || mode_a || mode_plus)
        flag |= FS_FLAG_TRUNCATE;
    if(mode_r && !mode_w && !mode_a && !mode_plus)
        flag |= FS_FLAG_READONLY; // "r"
    else if(mode_w && !mode_plus)
        flag |= FS_FLAG_WRITEONLY; // "w"

    file->fd = open(file->path, flag);
    if(file->fd == -1) {
        debug_println("fopen failed");
        free(file->path);
        file->path = NULL;
        return NULL;
    }

    // handle streams
    if(strncmp(file->path, "/dev/", 5) == 0) {
        file->is_stream = 1;
        file->buffer = NULL;
        file->size = 0;
        file->content_size = 0;
        file->position = 0;
        file->is_open = 1;
        file->dirty = 0;
        return file;
    }

    if(mode_r || mode_a) {
        // append or rw, read in current file contents but allocate an extra 4096 bytes
        // todo: lazy loading
        int size = fsize(file->fd);
        file->buffer = malloc(size + 0x1000);
        if(!file->buffer) {
            free(file->path);
            file->path = NULL;
            return NULL;
        }
        if(size > 0)
            read(file->fd, (char*)file->buffer, size);
        file->size = size + 0x1000;
        file->content_size = size;
        file->position = mode_a ? (uint32_t)size : 0; // append starts at eof
    } else {
        // w/w+
        file->buffer = (uint8_t*)malloc(0x1000);
        if(!file->buffer) {
            free(file->path);
            file->path = NULL;
            return NULL;
        }
        file->size = 0x1000;
        file->content_size = 0;
        file->position = 0;
    }
    
    file->is_open = 1;
    file->dirty = 0;
    
    return file;
}

size_t fwrite(const void *ptr, size_t size, size_t count, FILE *stream) {
    if(!stream || !stream->is_open) return 0;

    size_t total_bytes = size * count;

    // streams: no buffering
    if(stream->is_stream) {
        if(total_bytes == 0) return 0;
        int n = write(stream->fd, (char*)ptr, total_bytes);
        if(n <= 0) return 0;
        return (size_t)n / size;
    }

    debug_println("fwrite: Position %i bytes %i size %i", stream->position, total_bytes, stream->size);

    // expand buffer if needed
    if(stream->position + total_bytes > stream->size) {
        uint32_t new_size = stream->position + total_bytes + 0x1000;
        uint8_t *new_buffer = malloc(new_size);
        if(!new_buffer) return 0; // resize failed
        memset(new_buffer, 0, new_size);
        memcpy(new_buffer, stream->buffer, stream->content_size);
        free(stream->buffer);
        stream->buffer = new_buffer;
        stream->size = new_size;
    }
    
    // copy data to buffer
    memcpy(stream->buffer + stream->position, ptr, total_bytes);
    if(stream->position + total_bytes > stream->content_size) {
        stream->content_size = stream->position + total_bytes;
    }
    stream->position += total_bytes;
    stream->dirty = 1;
    
    return count;
}

size_t fread(void *ptr, size_t size, size_t count, FILE *stream) {
    if(!stream || !stream->is_open) return 0;

    size_t total_bytes = size * count;

    // streams: read straight from the fd (blocks for stdin/pipes)
    if(stream->is_stream) {
        if(total_bytes == 0) return 0;
        int n = read(stream->fd, (char*)ptr, total_bytes);
        if(n <= 0) return 0;
        return (size_t)n / size;
    }

    size_t available = stream->size - stream->position;
    
    if(total_bytes > available) {
        total_bytes = available;
        count = total_bytes / size;
    }
    memcpy(ptr, stream->buffer + stream->position, total_bytes);
    stream->position += total_bytes;
    
    return count;
}

int fclose(FILE *stream) {
    if(!stream || !stream->is_open) return -1;
    
    // if file was modified, write it back
    if(stream->dirty && (strchr(stream->mode, 'w') || strchr(stream->mode, 'a') || strchr(stream->mode, '+'))) {
        debug_println("Writing %u bytes", stream->content_size);
        write(stream->fd, (char*)stream->buffer, stream->content_size);
    }
    
    // Clean up
    if(stream->path)
        free(stream->path);
    if(stream->buffer)
        free(stream->buffer);
    
    memset(stream, 0, sizeof(FILE));
    
    return 0;
}

int fflush(FILE *stream) {
    if(!stream || !stream->is_open) return -1;
    
    if(stream->dirty && (strchr(stream->mode, 'w') || strchr(stream->mode, 'a') || strchr(stream->mode, '+'))) {
        write(stream->fd, (char*)stream->buffer, stream->content_size);
        stream->dirty = 0;
    }
    
    return 0;
}

int fileno(FILE *stream) {
    if(!stream)
        return -1;
    return stream->fd;
}

void fseek(FILE *stream, int pos, int type) {
    if(!stream) return;

    if(type == SEEK_SET)
        stream->position = pos;
    if(type == SEEK_CUR)
        stream->position += pos;
    if(type == SEEK_END)
        stream->position = stream->content_size;
}

void debug_println(const char *format, ...) {
   char buffer[1024];
   va_list args;
   va_start(args, format);
   vsnprintf(buffer, 1023, (char*)format, args);
   va_end(args);
   debug_write_str(buffer);
}

void printf(const char *format, ...) {
   char buffer[1024];
   va_list args;
   va_start(args, format);
   vsnprintf(buffer, 1023, (char*)format, args);
   va_end(args);
   write_str(buffer);
}

void printf_w(const char *format, int window, ...) {
   char buffer[1024];
   va_list args;
   va_start(args, window);
   vsnprintf(buffer, 1023, (char*)format, args);
   va_end(args);
   write_str_w(buffer, window);
}

void vfprintf(FILE *stream, const char *format, va_list args) {
   if(!stream) return;
   char buffer[1024];
   vsnprintf(buffer, 1023, (char*)format, args);
   fwrite(buffer, 1, strlen(buffer), stream);
}

void fprintf(FILE *stream, const char *format, ...) {
   if(!stream) return;
   va_list args;
   va_start(args, format);
   vfprintf(stream, format, args);
   va_end(args);
}

int ftell(FILE *stream) {
    if(!stream) return -1;
    return stream->position;
}
