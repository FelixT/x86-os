// libc style file io functions

#include "stdio.h"
#include "../prog.h"
#include "../../lib/string.h"
#include <stddef.h>

typedef struct {
   uint8_t filename[11];
   uint8_t attributes;
   uint8_t reserved;
   uint8_t creationTimeFine; // in 10ths of a second
   uint16_t creationTime;
   uint16_t creationDate;
   uint16_t lastAccessDate;
   uint16_t zero; // high 16 bits of entry's first cluster no in other fat vers
   uint16_t lastModifyTime;
   uint16_t lastModifyDate;
   uint16_t firstClusterNo;
   uint32_t fileSize; // bytes
} __attribute__((packed)) fat_dir_t;

#define MAX_FILES 16
static FILE file_table[MAX_FILES];
static int files_initialized = 0;

void memcpy(void *dest, const void *src, int bytes) {
   for(int i = 0; i < bytes; i++)
      ((uint8_t*)dest)[i] = ((uint8_t*)src)[i];
}

void init_files(void) {
    if (!files_initialized) {
        memset(file_table, 0, sizeof(file_table));
        files_initialized = 1;
    }
}

FILE* get_free_file(void) {
    init_files();
    for (int i = 0; i < MAX_FILES; i++) {
        if (!file_table[i].is_open) {
            return &file_table[i];
        }
    }
    return NULL;
}

FILE* fopen(const char* filename, const char* mode) {
    FILE* file = get_free_file();
    if (!file) return NULL;
    
    // Copy filename
    file->path = (char*)malloc(512);
    if (!file->path) return NULL;
    strcpy(file->path, (char*)filename);
    
    // Copy mode
    strncpy(file->mode, mode, sizeof(file->mode) - 1);
    file->mode[sizeof(file->mode) - 1] = '\0';
    
    if (strchr(mode, 'r')) {
        // for now, just read the whole buffer into the file object at init
        fat_dir_t *entry = (fat_dir_t*)fat_parse_path(file->path, true);
        if(!entry) {
            free((uint32_t)file->path, 512);
            return NULL;
        }
        file->buffer = (uint8_t*)fat_read_file(entry->firstClusterNo, entry->fileSize);
        file->size = entry->fileSize;
        file->content_size = entry->fileSize;
        file->position = 0;
    } else if(strchr(mode, 'w')) {
         // For write mode
        file->buffer = (uint8_t*)malloc(0x1000);
        if(!file->buffer) {
            free((uint32_t)file->path, 512);
            return NULL;
        }
        file->size = 4096;
        file->position = 0;
        file->content_size = 0;
    } else if(strchr(mode, 'a')) {
       // for now, just read the whole buffer into the file object at init
        fat_dir_t *entry = (fat_dir_t*)fat_parse_path(file->path, true);
        if(!entry) {
            free((uint32_t)file->path, 512);
            return NULL;
        }
        file->buffer = (uint8_t*)fat_read_file(entry->firstClusterNo, entry->fileSize);
        file->size = entry->fileSize + 0x1000;
        uint8_t *newbuf = (uint8_t*)malloc(file->size);
        memcpy(newbuf, file->buffer, entry->fileSize);
        free((uint32_t)file->buffer, entry->fileSize);
        file->buffer = newbuf;
        file->position = entry->fileSize;
        file->content_size = entry->fileSize;
    } else {
        debug_write_str("fopen: unsupported write mode\n");
        free((uint32_t)file->path, 512);
        return NULL;
    }
    
    file->is_open = 1;
    file->dirty = 0;
    
    return file;
}

size_t fwrite(const void* ptr, size_t size, size_t count, FILE* stream) {
    if (!stream || !stream->is_open) return 0;
    
    size_t total_bytes = size * count;
    
    // Expand buffer if needed
    if(stream->position + total_bytes > stream->size) {
        debug_write_str("Resizing not implemented\n");
        return 0;
        /*uint32_t new_size = stream->position + total_bytes + 1024;
        uint8_t *new_buffer = realloc(stream->buffer, new_size);
        if (!new_buffer) return 0;
        stream->buffer = new_buffer;
        stream->size = new_size;*/
    }
    
    // Copy data to buffer
    memcpy(stream->buffer + stream->position, ptr, total_bytes);
    stream->position += total_bytes;
    stream->content_size += total_bytes;
    stream->dirty = 1;
    
    return count;
}

size_t fread(void* ptr, size_t size, size_t count, FILE* stream) {
    if (!stream || !stream->is_open) return 0;
    
    size_t total_bytes = size * count;
    size_t available = stream->size - stream->position;
    
    if(total_bytes > available) {
        total_bytes = available;
        count = total_bytes / size;
    }
    
    memcpy(ptr, stream->buffer + stream->position, total_bytes);
    stream->position += total_bytes;
    
    return count;
}

int fclose(FILE* stream) {
    if(!stream || !stream->is_open) return -1;
    
    // If file was modified, write it back
    if(stream->dirty && (strchr(stream->mode, 'w') || strchr(stream->mode, 'a'))) {
        char buffer[200];
        sprintf(buffer, "Writing %u bytes\n", stream->content_size);
        debug_write_str(buffer);
        fat_write_file(stream->path, stream->buffer, stream->content_size);
    }
    
    // Clean up
    if(stream->path)
        free((uint32_t)stream->path, 512);
    if(stream->buffer)
        free((uint32_t)stream->buffer, stream->size);
    
    memset(stream, 0, sizeof(FILE));
    
    return 0;
}

int fflush(FILE* stream) {
    if(!stream || !stream->is_open) return -1;
    
    if(stream->dirty && (strchr(stream->mode, 'w') || strchr(stream->mode, 'a'))) {
        fat_write_file(stream->path, stream->buffer, stream->content_size);
        stream->dirty = 0;
    }
    
    return 0;
}