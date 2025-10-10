#ifndef FS_H
#define FS_H

#include <stdbool.h>
#include <stdint.h>
#include <stddef.h>

#define FS_MAX_FILENAME 256

typedef struct {
    uint32_t file_size;
    uint32_t first_cluster;
} fs_file_data_t;

typedef struct {
    bool active;
    char filename[FS_MAX_FILENAME];
    int flags;
    uint8_t type; // 0 = file, 1 = dir, 2 = term
    int window_index; // terminal window for terms
    uint32_t current_pos;
    fs_file_data_t *data;
} fs_file_t;

typedef struct {
    char filename[FS_MAX_FILENAME];
    int type;
    uint32_t file_size;
    bool hidden;
} fs_dir_entry_t; // simplified struct for listing directory contents

typedef struct {
    fs_dir_entry_t *entries;
    int size;
} fs_dir_content_t;


#define FS_TYPE_FILE 0
#define FS_TYPE_DIR 1
#define FS_TYPE_TERM 2

fs_file_t *fs_open(char *path);
bool fs_write(fs_file_t *file, uint8_t *buffer, uint32_t size);
int fs_read(fs_file_t *file, void *buffer, size_t size, void *callback, int task);
bool fs_mkdir(char *path);
fs_file_t *fs_new(char *path);
bool fs_rename(char *oldpath, char *newname);
fs_dir_content_t *fs_read_dir(char *path);
void fs_dir_content_free(fs_dir_content_t *content);
int fs_filesize(fs_file_t *file);

#endif