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

#define FS_PIPE_BUF_SIZE 4096

typedef struct {
    uint8_t buf[FS_PIPE_BUF_SIZE];
    int read_pos;
    int write_pos;
    int size;
    int writer_count; // eof when this hits 0
    int ref_count;
    int read_waiting_task;
    int write_waiting_task;
    uint32_t read_waiting_uid;
    uint32_t write_waiting_uid;
    void *read_buf;
    size_t read_size;
    void *write_buf;
    size_t write_size;
} fs_pipe_t;

typedef struct {
    bool active;
    char filename[FS_MAX_FILENAME];
    int flags;
    uint8_t type;
    int window_index; // terminal window for terms
    uint32_t current_pos;
    fs_file_data_t *data;
    fs_pipe_t *pipe;
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
#define FS_TYPE_PIPE 3

#define SEEK_SET 0
#define SEEK_CUR 1
#define SEEK_END 2

#define FS_FLAG_WRITEONLY 1
#define FS_FLAG_READONLY 2

// returns
#define FS_EOF 0
#define FS_ERROR -1
#define FS_BLOCKING -2
#define FS_WRITE_WAIT -3

fs_file_t *fs_open(char *path);
fs_file_t *fs_dup(fs_file_t *file);
void fs_close(fs_file_t *file);
int fs_write(fs_file_t *file, uint8_t *buffer, uint32_t size, int task);
int fs_read(fs_file_t *file, void *buffer, size_t size, void *callback, int task);
bool fs_mkdir(char *path);
fs_file_t *fs_new(char *path);
bool fs_unlink(char *path);
bool fs_rmdir(char *path);
bool fs_rename(char *oldpath, char *newname);
fs_dir_content_t *fs_read_dir(char *path);
void fs_dir_content_free(fs_dir_content_t *content);
int fs_filesize(fs_file_t *file);
int fs_seek(fs_file_t *file, int offset, int type);
void fs_create_pipe(fs_file_t **read_end, fs_file_t **write_end);
bool fs_pipe_wake_reader(fs_pipe_t *pipe);
bool fs_pipe_wake_writer(fs_pipe_t *pipe);

#endif