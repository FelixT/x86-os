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
    
    // copy mode
    strncpy(file->mode, mode, sizeof(file->mode) - 1);
    file->mode[sizeof(file->mode) - 1] = '\0';
    file->is_stream = 0;
    
    bool mode_r = strchr(mode, 'r') != NULL;
    bool mode_w = strchr(mode, 'w') != NULL;
    bool mode_a = strchr(mode, 'a') != NULL;
    bool mode_plus = strchr(mode, '+') != NULL;

    if(!mode_r && !mode_w && !mode_a) {
        debug_write_str("fopen: invalid mode\n");
        return NULL;
    }

    file->buffer_pos = 0;
    file->buffer = NULL; // lazy allocated on first write

    if(strncmp((char*)filename, "/dev/", 5) == 0) {
        file->fd = open((char*)filename, mode_r ? FS_FLAG_READONLY : FS_FLAG_WRITEONLY);
        if(file->fd == -1) return NULL;
        file->is_stream = 1;
        file->is_open = 1;
        return file;
    }

    int flag = 0;
    if(!mode_plus)
        flag |= mode_r ? FS_FLAG_READONLY : FS_FLAG_WRITEONLY; // "r" / "w" / "a"
    if(mode_w || mode_a)
        flag |= FS_FLAG_CREATE;
    if(mode_a)
        flag |= FS_FLAG_APPEND; // writes always go to eof
    if(mode_w)
        flag |= FS_FLAG_TRUNCATE; // "w"/"w+": existing contents are dropped on open

    file->fd = open((char*)filename, flag);
    if(file->fd == -1) {
        debug_println("fopen failed");
        return NULL;
    }

    file->is_open = 1;
    return file;
}

size_t fwrite(const void *ptr, size_t size, size_t count, FILE *stream) {
    if(!stream || !stream->is_open || size == 0) return 0;

    size_t total_bytes = size * count;
    if(total_bytes == 0) return 0;

    // unbuffered
    if(stream->is_stream) {
        int n = write(stream->fd, (char*)ptr, total_bytes);
        if(n < 0) return 0;
        return (size_t)n / size;
    }

    if(!stream->buffer) {
        stream->buffer = malloc(STDIO_BUFFER_SIZE);
        if(!stream->buffer) return 0;
        stream->buffer_pos = 0;
    }

    uint32_t written_bytes = 0;
    uint32_t remaining_bytes = total_bytes;
    while(remaining_bytes > 0) {
        uint32_t capacity = STDIO_BUFFER_SIZE - stream->buffer_pos;
        uint32_t write_bytes = (remaining_bytes > STDIO_BUFFER_SIZE) ? STDIO_BUFFER_SIZE : remaining_bytes;
        if(write_bytes > capacity) {
            write_bytes = capacity;
        }
        if(write_bytes == 0) {
            if(fflush(stream) < 0) {
                return written_bytes / size;
            }
            continue;
        }

        memcpy(stream->buffer + stream->buffer_pos, (uint8_t*)ptr + written_bytes, write_bytes);
        stream->buffer_pos += write_bytes;
        written_bytes += write_bytes;
        remaining_bytes -= write_bytes;
    }

    return written_bytes / size;
}

size_t fread(void *ptr, size_t size, size_t count, FILE *stream) {
    if(!stream || !stream->is_open || size == 0) return 0;

    size_t total_bytes = size * count;
    if(total_bytes == 0) return 0;

    // flush writes before reading
    if(fflush(stream) < 0)
        return 0;

    if(stream->is_stream) {
        int n = read(stream->fd, (char*)ptr, total_bytes);
        if(n <= 0) return 0;
        return (size_t)n / size;
    }


    size_t read_bytes = 0;
    while(read_bytes < total_bytes) {
        int n = read(stream->fd, (char*)ptr + read_bytes, total_bytes - read_bytes);
        if(n <= 0) break;
        read_bytes += (size_t)n;
    }

    return read_bytes / size;
}

int fclose(FILE *stream) {
    if(!stream || !stream->is_open) return -1;

    int result = (fflush(stream) < 0) ? -1 : 0;
    close(stream->fd);
    if(stream->buffer) {
        free(stream->buffer);
        stream->buffer = NULL;
    }
    memset(stream, 0, sizeof(FILE));

    return result; // flush successful
}

void fclose_all() {
    if(!files_initialized) return; // nothing was ever opened
    for(int i = 0; i < MAX_FILES; i++) {
        if(file_table[i].is_open)
            fclose(&file_table[i]);
    }
}

int fflush(FILE *stream) {
    if(!stream || !stream->is_open) return -1;
    if(!stream->buffer || stream->buffer_pos == 0) return 0; // nothing to flush

    uint32_t sent = 0;
    while(sent < stream->buffer_pos) {
        int n = write(stream->fd, (char*)stream->buffer + sent, stream->buffer_pos - sent);
        if(n <= 0) break; // error
        sent += (uint32_t)n;
    }

    if(sent < stream->buffer_pos) {
        // in case of error, keep bytes awaiting flush
        memmove(stream->buffer, stream->buffer + sent, stream->buffer_pos - sent);
        stream->buffer_pos -= sent;
        return -1;
    }

    stream->buffer_pos = 0;

    return 0;
}

int fileno(FILE *stream) {
    if(!stream)
        return -1;
    return stream->fd;
}

int fseek(FILE *stream, int pos, int type) {
    if(!stream || !stream->is_open || stream->is_stream) return -1;
    if(fflush(stream) < 0) {
        debug_write_str("fseek: flush failed, not seeking\n");
        return -1;
    }
    if(seek(stream->fd, pos, type) < 0) {
        debug_write_str("fseek: seek failed\n");
        return -1;
    }
    return 0;
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
    if(!stream || !stream->is_open) return -1;
    if(stream->is_stream) return 0;
    int size;
    if(strchr(stream->mode, 'a') != NULL && !(strchr(stream->mode, '+') != NULL))
        size = fsize(stream->fd);
    else
        size = seek(stream->fd, 0, SEEK_CUR);
    if(size < 0) return -1;
    return size + stream->buffer_pos;
}
