#include "fs.h"
#include "fat.h"
#include "memory.h"
#include "windowmgr.h"
#include "window.h"

// interface for interacting with fat16 fs and terminal as files

fs_file_t *fs_open(char *path) {
   if(path == NULL || strlen(path) == 0 || strlen(path) > 255) {
      debug_printf("FS: invalid file path\n");
      return NULL;
   }
   fs_file_t *file = (fs_file_t*)malloc(sizeof(fs_file_t));
   fs_file_data_t *data = (fs_file_data_t*)malloc(sizeof(fs_file_data_t));
   file->active = true;
   file->data = data;
   file->current_pos = 0;
   strcpy(file->filename, path);
   if(strequ(path, "/dev/stdin") || strequ(path, "/dev/stdout") || strequ(path, "/dev/stderr")) {
      file->type = FS_TYPE_TERM;
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

   if(entry->attributes & 0x10) {
      file->type = FS_TYPE_DIR;
   } else {
      file->type = FS_TYPE_FILE;
   }
   data->file_size = entry->fileSize;
   data->first_cluster = entry->firstClusterNo;
   file->data = data;
   free((uint32_t)entry, sizeof(fat_dir_t));
   return file;
}

bool fs_mkdir(char *path) {
   if(path == NULL || strlen(path) == 0 || strlen(path) > 255) {
      debug_printf("FS: invalid dir path\n");
      return false;
   }
   debug_printf("FS: creating new dir '%s'\n", path);
   return fat_new_dir(path);
}

fs_file_t *fs_new(char *path) {
   if(path == NULL || strlen(path) == 0 || strlen(path) > 255) {
      debug_printf("FS: invalid file path\n");
      return NULL;
   }
   debug_printf("FS: creating new file '%s'\n", path);
   fat_dir_t *entry = fat_parse_path(path, true);
   if(entry) {
      free((uint32_t)entry, sizeof(fat_dir_t));
      debug_printf("FS: file %s already exists\n", path);
      return NULL;
   }
   fat_new_file(path);
   return fs_open(path);
}

bool fs_write(fs_file_t *file, uint8_t *buffer, uint32_t size) {
   if(file->type == FS_TYPE_TERM) {
      debug_printf("Write %u to window %i\n", size, file->window_index);
      window_writestrn((char*)buffer, size, 0, file->window_index);
   } else if(file->type == FS_TYPE_FILE) {
      // write to file
      if(fat_write_file(file->filename, buffer, size) < 0) {
         debug_printf("FS: error writing to file %s\n", file->filename);
         return false;
      }
   } else {
      debug_printf("FS: cannot write to directory %s\n", file->filename);
      return false;
   }

   return true;
}

bool fs_read(fs_file_t *file, void *buffer, size_t size, void *callback, int task) {
   if((int)size < 0)
      size = file->data->file_size;
   if(file->current_pos + size > file->data->file_size)
      size = file->data->file_size - file->current_pos;
   if(size == 0) return false;

   debug_printf("Reading %u bytes to buffer at 0x%h\n", size, buffer);

   fat_read_file_chunked(file->data->first_cluster, buffer, size, callback, task);
   file->current_pos += size;
   return true;
}

bool fs_rename(char *oldpath, char *newname) {
   if(oldpath == NULL || newname == NULL) {
      debug_printf("FS: invalid old path or new name\n");
      return false;
   }
   if(strlen(newname) == 0 || strlen(newname) > 11) {
      debug_printf("FS: invalid new name '%s'\n", newname);
      return false;
   }
   return fat_rename(oldpath, newname);
} 