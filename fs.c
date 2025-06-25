#include "fs.h"
#include "fat.h"
#include "memory.h"
#include "windowmgr.h"
#include "window.h"

// interface for interacting with fat16 fs and terminal as files

fs_file_t *fs_open(char *path) {
   fs_file_t *file = (fs_file_t*)malloc(sizeof(fs_file_t));
   file->active = true;
   file->current_pos = 0;
   strcpy(file->filename, path);
   if(strcmp(path, "/dev/stdin") || strcmp(path, "/dev/stdout") || strcmp(path, "/dev/stderr")) {
      file->is_term = true;
      file->window_index = getSelectedWindowIndex();
      return file;
   }

   fat_dir_t *entry = fat_parse_path(path, true);
   if(!entry) {
      free((uint32_t)entry, sizeof(fat_dir_t));
      free((uint32_t)file, sizeof(fs_file_t));
      debug_printf("FS: file %s not found\n", path);
      return NULL;
   }

   file->file_size = entry->fileSize;
   file->is_dir = (entry->attributes & 0x10) == 0;
   file->first_cluster = entry->firstClusterNo;
   free((uint32_t)entry, sizeof(fat_dir_t));
   return file;
}

void fs_write(fs_file_t *file, uint8_t *buffer, uint32_t size) {
   if(file->is_term) {
      debug_printf("Write %u to window %i\n", size, file->window_index);
      window_writestrn((char*)buffer, size, 0, file->window_index);
   } else {
      fat_write_file(file->filename, buffer, size);
   }
}

bool fs_read(fs_file_t *file, void *buffer, size_t size, void *callback, int task) {
   if((int)size < 0)
      size = file->file_size;
   if(file->current_pos + size > file->file_size)
      size = file->file_size - file->current_pos;
   if(size == 0) return false;

   debug_printf("Reading %u bytes to buffer at 0x%h\n", size, buffer);

   fat_read_file_chunked(file->first_cluster, buffer, size, callback, task);
   file->current_pos += size;
   return true;
}