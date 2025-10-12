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

fs_dir_entry_t fs_get_dir_entry(fat_dir_t *item) {
   fs_dir_entry_t entry;
   char fileName[9];
   char extension[4];
   strcpy_fixed((char*)fileName, (char*)item->filename, 8);
   strcpy_fixed((char*)extension, (char*)item->filename+8, 3);
   strsplit((char*)fileName, NULL, (char*)fileName, ' '); // null terminate at first space
   strsplit((char*)extension, NULL, (char*)extension, ' '); // null terminate at first space
   tolower((char*)fileName);
   tolower((char*)extension);
   if(extension[0] != '\0') {
      sprintf(entry.filename, "%s.%s", fileName, extension);
   } else {
      sprintf(entry.filename, "%s", fileName);
   }
   entry.type = (item->attributes & 0x10) ? FS_TYPE_DIR : FS_TYPE_FILE;
   entry.file_size = item->fileSize;
   entry.hidden = (item->attributes & 0x02) != 0;

   return entry;
}

fs_dir_content_t *fs_read_dir(char *path) {
   fs_dir_content_t *content = (fs_dir_content_t*)malloc(sizeof(fs_dir_content_t));
   content->entries = NULL;
   content->size = 0;

   if(strequ(path, "/") || strequ(path, "")) {
      // root
      fat_dir_t *items = fat_read_root();
      fat_bpb_t fat_bpb = fat_get_bpb();
      content->size = fat_bpb.noRootEntries;
      for(int i = 0; i < fat_bpb.noRootEntries; i++) {
         if(items[i].filename[0] == 0) {
            content->size = i;
            break;
         }
      }
      content->entries = (fs_dir_entry_t*)malloc(sizeof(fs_dir_entry_t) * content->size);
      for(int i = 0; i < content->size; i++) {
         fs_dir_entry_t *entry = &content->entries[i];
         *entry = fs_get_dir_entry(&items[i]);
      }
      free((uint32_t)items, sizeof(fat_dir_t) * fat_bpb.noRootEntries);
      return content;
   } else {
      fat_dir_t *entry = (fat_dir_t*)fat_parse_path(path, true);
      if(entry == NULL)
         return content;
      if(entry->attributes & 0x10) {
         int size = fat_get_dir_size((uint16_t) entry->firstClusterNo);
         fat_dir_t *items = malloc(size*sizeof(fat_dir_t));
         fat_read_dir(entry->firstClusterNo, items);
         content->size = size;
         for(int i = 0; i < size; i++) {
            if(items[i].filename[0] == 0) {
               content->size = i;
               break;
            }
         }
         content->entries = (fs_dir_entry_t*)malloc(sizeof(fs_dir_entry_t) * content->size);
         for(int i = 0; i < content->size; i++)
         {
            fs_dir_entry_t *entry = &content->entries[i];
            *entry = fs_get_dir_entry(&items[i]);
         }
         free((uint32_t)items, size*sizeof(fat_dir_t));
      } else {
         return content;
      }
      free((uint32_t)entry, sizeof(fat_dir_t));
      return content;
   }
}

void fs_dir_content_free(fs_dir_content_t *content) {
   if(content) {
      if(content->entries)
         free((uint32_t)content->entries, sizeof(fs_dir_entry_t) * content->size);
      free((uint32_t)content, sizeof(fs_dir_content_t));
   }
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
      //debug_printf("Write %u to window %i\n", size, file->window_index);
      int w = file->window_index;
      if(w > 0 && w < getWindowCount() && !getWindow(w)->closed) {
         window_writestrn((char*)buffer, size, 0, file->window_index);
      } else {
         debug_printf("FS: error writing to window %s\n", file->window_index);
      }
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

int fs_read(fs_file_t *file, void *buffer, size_t size, void *callback, int task) {
   if((int)size < 0)
      size = file->data->file_size;
   if(file->current_pos + size > file->data->file_size)
      size = file->data->file_size - file->current_pos;
   if(size == 0) return 0;

   debug_printf("Reading %u bytes to buffer at 0x%h\n", size, buffer);

   fat_read_file_chunked(file->data->first_cluster, buffer, size, callback, task);
   file->current_pos += size;
   return size;
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

int fs_filesize(fs_file_t *file) {
   return file->data->file_size;
}