#ifndef FS_H
#define FS_H

#include <stdbool.h>
#include <stdint.h>
#include <stddef.h>

#define FS_MAX_FILENAME 256

typedef struct {
    bool active;
    char filename[FS_MAX_FILENAME];
    uint32_t file_size; // rw position
    uint32_t current_pos;
    int flags;
    bool is_dir;
    uint32_t first_cluster;
    bool is_term;
    int window_index;
} fs_file_t;

fs_file_t *fs_open(char *path);
void fs_write(fs_file_t *file, uint8_t *buffer, uint32_t size);
bool fs_read(fs_file_t *file, void *buffer, size_t size, void *callback, int task);

#endif