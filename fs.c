#include "fs.h"
#include "fat.h"
#include "memory.h"
#include "windowmgr.h"
#include "window.h"

// interface for interacting with fat16 fs and terminal as files from usermode

fs_file_t *fs_open(char *path) {
   if(path == NULL || strlen(path) == 0 || strlen(path) > 255) {
      debug_printf("FS: invalid file path\n");
      return NULL;
   }
   fs_file_t *file = (fs_file_t*)malloc(sizeof(fs_file_t));
   fs_file_data_t *data = (fs_file_data_t*)malloc(sizeof(fs_file_data_t));
   file->active = true;
   file->data = data;
   file->pipe = NULL;
   file->current_pos = 0;
   strcpy(file->filename, path);

   // terminals
   if(strequ(path, "/dev/stdin") || strequ(path, "/dev/stdout") || strequ(path, "/dev/stderr")) {
      file->type = FS_TYPE_TERM;
      file->window_index = getSelectedWindowIndex();
      if(strequ(path, "/dev/stdin"))
         file->flags = FS_FLAG_READONLY;
      else
         file->flags = FS_FLAG_WRITEONLY;
      return file;
   }

   // files
   fat_dir_t *entry = fat_parse_path(path, true);
   if(!entry) {
      free((uint32_t)file, sizeof(fs_file_t));
      free((uint32_t)data, sizeof(fs_file_data_t));
      debug_printf("FS: file %s not found\n", path);
      return NULL;
   }

   if(entry->attributes & 0x10) {
      file->type = FS_TYPE_DIR;
   } else {
      file->type = FS_TYPE_FILE;
   }
   file->flags = 0;
   data->file_size = entry->fileSize;
   data->first_cluster = entry->firstClusterNo;
   file->data = data;
   free((uint32_t)entry, sizeof(fat_dir_t));
   return file;
}

void fs_close(fs_file_t *file) {
   if(!file) return;
   if(file->data)
      free((uint32_t)file->data, sizeof(fs_file_data_t));
   if(file->pipe) {
      if(file->flags & FS_FLAG_WRITEONLY)
         file->pipe->writer_count--;
      if(file->pipe->read_waiting_task != -1) {
         task_state_t *task = &gettasks()[file->pipe->read_waiting_task];
         if(task->enabled && task->task_uid == file->pipe->read_waiting_uid) {
            task->paused = false;
            task->registers.ebx = FS_EOF;
         }
         file->pipe->read_waiting_task = -1;
      }
      if(file->pipe->write_waiting_task != -1) {
         task_state_t *task = &gettasks()[file->pipe->write_waiting_task];
         if(task->enabled && task->task_uid == file->pipe->write_waiting_uid) {
            task->paused = false;
            task->registers.ebx = FS_EOF;
         }
         file->pipe->write_waiting_task = -1;
         file->pipe->write_buf = NULL;
         file->pipe->write_size = 0;
      }
      if(file->pipe->ref_count > 0)
         file->pipe->ref_count--;
      if(file->pipe->ref_count == 0)
         free((uint32_t)file->pipe, sizeof(fs_pipe_t));
   }
   free((uint32_t)file, sizeof(fs_file_t));
}

fs_file_t *fs_dup(fs_file_t *file) {
   if(!file) return NULL;
   fs_file_t *dup = (fs_file_t*)malloc(sizeof(fs_file_t));
   *dup = *file;
   if(file->data) {
      fs_file_data_t *data = (fs_file_data_t*)malloc(sizeof(fs_file_data_t));
      *data = *file->data;
      dup->data = data;
   }
   if(file->pipe) {
      dup->pipe = file->pipe;
      dup->pipe->ref_count++;
      if(dup->flags & FS_FLAG_WRITEONLY)
         dup->pipe->writer_count++;
   }
   return dup;
}

fs_dir_entry_t fs_get_dir_entry(fat_dir_t *item) {
   fs_dir_entry_t entry;
   char fileName[9];
   char extension[4];
   strcpy_fixed((char*)fileName, (char*)item->filename, 8);
   strcpy_fixed(extension, (char*)item->filename+8, 3);
   strsplit(fileName, NULL, (char*)fileName, ' '); // null terminate at first space
   strsplit(extension, NULL, (char*)extension, ' '); // null terminate at first space
   strtolower(fileName);
   strtolower(extension);
   if(extension[0] != '\0') {
      sprintf(entry.filename, "%s.%s", fileName, extension);
   } else {
      sprintf(entry.filename, "%s", fileName);
   }
   entry.type = (item->attributes & 0x10) ? FS_TYPE_DIR : FS_TYPE_FILE;
   entry.file_size = item->fileSize;
   entry.hidden = (item->attributes & 0x02) || (item->attributes & 0x08); // 'hidden' and 'volume' entries

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
      content->size = 0;
      for(int i = 0; i < fat_bpb.noRootEntries; i++) {
         if(items[i].filename[0] == 0) break;
         if(items[i].filename[0] == 0xE5) continue; // deleted entry
         content->size++;
      }
      content->entries = (fs_dir_entry_t*)malloc(sizeof(fs_dir_entry_t) * content->size);
      int out = 0;
      for(int i = 0; i < fat_bpb.noRootEntries && out < content->size; i++) {
         if(items[i].filename[0] == 0) break;
         if(items[i].filename[0] == 0xE5) continue; // deleted entry
         content->entries[out++] = fs_get_dir_entry(&items[i]);
      }
      free((uint32_t)items, sizeof(fat_dir_t) * fat_bpb.noRootEntries);
      return content;
   } else {
      // not root

      fat_dir_t *entry = (fat_dir_t*)fat_parse_path(path, true);
      if(entry == NULL) {
         // not found
         free((uint32_t)content, sizeof(fat_dir_t));
         return NULL;
      }

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
         for(int i = 0; i < content->size; i++) {
            fs_dir_entry_t *entry = &content->entries[i];
            *entry = fs_get_dir_entry(&items[i]);
         }
         free((uint32_t)items, size*sizeof(fat_dir_t));
      } else {
         // not a dir
         free((uint32_t)entry, sizeof(fat_dir_t));
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
   if(!fat_new_file(path))
      return NULL;
   return fs_open(path);
}

// copy up to max bytes from pipe ring buffer into task (reader)
static size_t fs_pipe_drain(fs_pipe_t *pipe, int task, void *dest, size_t max) {
   size_t read = 0;
   // handle wrap - read in continuous chunks validated by copy_to_task
   while(read < max && pipe->size > 0) {
      size_t run = FS_PIPE_BUF_SIZE - pipe->read_pos; // contiguous bytes until the ring wraps
      size_t chunk = max - read;
      if(chunk > run) chunk = run;
      if(chunk > (size_t)pipe->size) chunk = pipe->size;
      int written = copy_to_task(task, (uint8_t*)dest + read, &pipe->buf[pipe->read_pos], chunk);
      if(written < 0)
         break; // bad/unmapped user buffer
      pipe->read_pos = (pipe->read_pos + written) % FS_PIPE_BUF_SIZE;
      pipe->size -= written;
      read += written;
      if((size_t)written < chunk)
         break; // ran out of mapping mid chunk
   }
   return read;
}

// copy up to max bytes from task (writer) buffer into pipe ring buffer
static size_t fs_pipe_fill(fs_pipe_t *pipe, int task, void *src, size_t max) {
   // write in continuous chunks validated in copy_from_task
   size_t written = 0;
   while(written < max && pipe->size < FS_PIPE_BUF_SIZE) {
      size_t run = FS_PIPE_BUF_SIZE - pipe->write_pos;
      size_t space = FS_PIPE_BUF_SIZE - pipe->size;
      size_t chunk = max - written;
      if(chunk > run) chunk = run;
      if(chunk > space) chunk = space;
      int read = copy_from_task(task, &pipe->buf[pipe->write_pos], (uint8_t*)src + written, chunk);
      if(read < 0)
         break; // bad/unmapped user buffer
      pipe->write_pos = (pipe->write_pos + read) % FS_PIPE_BUF_SIZE;
      pipe->size += read;
      written += read;
      if((size_t)read < chunk)
         break; // ran out of mapping mid chunk
   }
   return written;
}

int fs_write(fs_file_t *file, uint8_t *buffer, uint32_t size, int task) {
   if(file->type == FS_TYPE_TERM) {
      int w = file->window_index;
      if(w > 0 && w < getWindowCount() && !getWindow(w)->closed) {
         window_writestrn((char*)buffer, size, 0, file->window_index);
         return size;
      } else {
         debug_printf("FS: error writing to window %s\n", file->window_index);
         return FS_ERROR;
      }
   }
   
   if(file->type == FS_TYPE_FILE) {
      // write to file
      if(fat_write_file(file->filename, buffer, size) < 0) {
         debug_printf("FS: error writing to file %s\n", file->filename);
         return FS_ERROR;
      }
      return size;
   }
   
   if(file->type == FS_TYPE_PIPE) {
      fs_pipe_t *pipe = file->pipe;
      if(!pipe) {
         debug_printf("api_write: pipe not active\n");
         return FS_ERROR;
      }

      if(pipe->size == FS_PIPE_BUF_SIZE) {
         pipe->write_waiting_task = task;
         pipe->write_waiting_uid = gettasks()[task].task_uid;
         return FS_WRITE_WAIT;
      }

      size_t written = fs_pipe_fill(pipe, task, buffer, size);
      if(written == 0 && size > 0)
         return FS_ERROR; // invalid/unmapped buffer

      return (int)written;
   }

   if(file->type == FS_TYPE_DIR) {
      debug_printf("FS: cannot write to directory %s\n", file->filename);
      return FS_ERROR;
   }

   debug_printf("FS: invalid file type for writing %s\n", file->filename);
   return FS_ERROR;
}

int fs_read(fs_file_t *file, void *buffer, size_t size, void *callback, int task) {
   if(file->type == FS_TYPE_TERM) {
      gui_window_t *window = getWindow(file->window_index);
      if(window->closed) {
         debug_printf("FS: error reading from window %i\n", file->window_index);
         return FS_ERROR;
      }

      if(file->flags & FS_FLAG_WRITEONLY) {
         debug_printf("FS: cannot read from write-only file %s\n", file->filename);
         return FS_ERROR;
      }
      // note: only one task can read from a windows stdin at a time as these get overwritten
      window->read_func = callback;
      window->read_buffer = buffer;
      window->read_task = task;
      window->read_task_uid = gettasks()[task].task_uid;
      return FS_BLOCKING;
   }

   if(file->type == FS_TYPE_DIR) {
      debug_printf("FS: cannot read from directory %s\n", file->filename);
      return FS_ERROR;
   }

   if(file->type == FS_TYPE_PIPE) {
      fs_pipe_t *pipe = file->pipe;
      if(!pipe) {
         debug_printf("FS: pipe not active\n");
         return FS_ERROR;
      }
      size_t read = fs_pipe_drain(pipe, task, buffer, size);
      if(read > 0) {
         return read;
      } else if(pipe->size > 0 && size > 0) {
         return FS_ERROR; // invalid/unmapped buffer
      } else if(pipe->writer_count == 0) {
         return FS_EOF;
      } else {
         // wait for data
         pipe->read_waiting_task = task;
         pipe->read_waiting_uid = gettasks()[task].task_uid;
         return FS_BLOCKING;
      }
   }

   if(file->type == FS_TYPE_FILE) {
      if(file->flags & FS_FLAG_WRITEONLY) {
         debug_printf("FS: cannot read from write-only file %s\n", file->filename);
         return FS_ERROR;
      }
      if(file->current_pos >= file->data->file_size)
         return FS_EOF;
   } else {
      debug_printf("FS: cannot read from directory %s\n", file->filename);
      return FS_ERROR;
   }

   if((int)size < 0)
      size = file->data->file_size;
   if(file->current_pos + size > file->data->file_size)
      size = file->data->file_size - file->current_pos;
   if(size == 0) return FS_EOF;

   fat_read_file_chunked(file->data->first_cluster, buffer, file->current_pos, size, callback, task);
   file->current_pos += size;
   return FS_BLOCKING;
}

bool fs_unlink(char *path) {
   if(path == NULL || strlen(path) == 0 || strlen(path) > 255) return false;
   return fat_delete_file(path);
}

bool fs_rmdir(char *path) {
   if(path == NULL || strlen(path) == 0 || strlen(path) > 255) return false;
   return fat_delete_dir(path);
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

int fs_seek(fs_file_t *file, int offset, int type) {
   if(type == SEEK_SET)
      file->current_pos = offset;
   else if(type == SEEK_CUR)
      file->current_pos += offset;
   else if(type == SEEK_END)
      file->current_pos = file->data->file_size;
   else
      return -1;
   
   if(file->current_pos > file->data->file_size)
      file->current_pos = file->data->file_size;
   return file->current_pos;
}

void fs_create_pipe(fs_file_t **read_end, fs_file_t **write_end) {
   fs_pipe_t *pipe = (fs_pipe_t*)malloc(sizeof(fs_pipe_t));
   memset(pipe, 0, sizeof(fs_pipe_t));
   pipe->read_waiting_task = -1;
   pipe->write_waiting_task = -1;
   pipe->writer_count = 1;
   pipe->ref_count = 2;

   fs_file_t *read_file = (fs_file_t*)malloc(sizeof(fs_file_t));
   read_file->filename[0] = '\0';
   read_file->window_index = -1;
   read_file->current_pos = 0;
   read_file->data = NULL;
   read_file->active = true;
   read_file->type = FS_TYPE_PIPE;
   read_file->pipe = pipe;
   read_file->flags = 0;

   fs_file_t *write_file = (fs_file_t*)malloc(sizeof(fs_file_t));
   write_file->filename[0] = '\0';
   write_file->window_index = -1;
   write_file->current_pos = 0;
   write_file->data = NULL;
   write_file->active = true;
   write_file->type = FS_TYPE_PIPE;
   write_file->pipe = pipe;
   write_file->flags = FS_FLAG_WRITEONLY;

   *read_end = read_file;
   *write_end = write_file;
}

bool fs_pipe_wake_reader(fs_pipe_t *pipe) {
   int reader_task = pipe->read_waiting_task;
   if(reader_task < 0) return false;
   task_state_t *task = &gettasks()[reader_task];
   if(!task->enabled || task->task_uid != pipe->read_waiting_uid) {
      pipe->read_waiting_task = -1;
      return false;
   }
   uint8_t *rbuf = (uint8_t*)pipe->read_buf;
   size_t rsize = pipe->read_size;
   if(rbuf) {
      // read from pipe
      size_t n = fs_pipe_drain(pipe, reader_task, rbuf, rsize);
      task->registers.ebx = (int)n;
   } else {
      task->registers.ebx = FS_ERROR;
   }
   task->paused = false;
   pipe->read_waiting_task = -1;
   return true;
}

bool fs_pipe_wake_writer(fs_pipe_t *pipe) {
   int writer_task = pipe->write_waiting_task;
   if(writer_task < 0) return false;
   task_state_t *task = &gettasks()[writer_task];
   if(!task->enabled || task->task_uid != pipe->write_waiting_uid) {
      pipe->write_waiting_task = -1;
      return false;
   }
   uint8_t *wbuf = (uint8_t*)pipe->write_buf;
   size_t wsize = pipe->write_size;
   if(wbuf) {
      // write into pipe
      size_t n = fs_pipe_fill(pipe, writer_task, wbuf, wsize);
      task->registers.ebx = (int)n;
   } else {
      task->registers.ebx = FS_ERROR;
   }
   task->paused = false;
   pipe->write_waiting_task = -1;
   return true;
}