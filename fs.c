#include "fs.h"
#include "fat.h"
#include "memory.h"
#include "windowmgr.h"
#include "window.h"
#include "ksync.h"

// interface for interacting with fat16 fs and terminal as files

kmutex_t fs_mutex = {.owner = NULL, .waiters = NULL, .waiters_tail = NULL};

extern bool switching;

// read entire file
uint8_t *fs_read_file_kernel(char *path, int *size) {
   if(switching) // may be called before tasks are init
      kmutex_lock(&fs_mutex);
   
   fat_dir_t entry;
   if(!fat_parse_path(path, true, &entry) || entry.attributes == 0x10) {
      if(switching)
         kmutex_unlock(&fs_mutex);
      return NULL; // not found
   }
   *size = entry.fileSize;
   uint8_t *content = fat_read_file(entry.firstClusterNo, entry.fileSize);

   if(switching)
      kmutex_unlock(&fs_mutex);

   return content;
}

static bool fs_is_term_path(char *path) {
   return strequ(path, "/dev/stdin") || strequ(path, "/dev/stdout") || strequ(path, "/dev/stderr");
}

static void fs_open_term(process_t *process, fs_file_t *file) {
   file->type = FS_TYPE_TERM;
   file->window_index = process->window;
   file->window_uid = process->window >= 0 ? getWindow(process->window)->uid : 0;
   if(strequ(file->filename, "/dev/stdin"))
      file->flags = FS_FLAG_READONLY;
   else
      file->flags = FS_FLAG_WRITEONLY;
}

static bool fs_open_locked(fs_file_t *file);

static bool fs_new_locked(fs_file_t *file) {
   // assumes path already verified
   fat_dir_t entry;
   if(fat_parse_path(file->filename, true, &entry)) {
      debug_printf("FS: file %s already exists\n", file->filename);
      return false;
   }
   if(!fat_new_file(file->filename))
      return false;

   file->flags &= ~FS_FLAG_CREATE;
   bool success = fs_open_locked(file);
   file->flags |= FS_FLAG_CREATE;

   return success;
}

static bool fs_open_locked(fs_file_t *file) { 
   // assumes lock already claimed & path is validated

   fat_dir_t entry;
   if(!fat_parse_path(file->filename, true, &entry)) {
      if(file->flags & FS_FLAG_CREATE) {
         debug_printf("FS: creating new file %s\n", file->filename);
         return fs_new_locked(file);
      } else {
         return false;
      }
   }

   fs_file_data_t *data = (fs_file_data_t*)malloc(sizeof(fs_file_data_t));

   if(entry.attributes & 0x10) {
      file->type = FS_TYPE_DIR;
   } else {
      file->type = FS_TYPE_FILE;
   }
   data->file_size = entry.fileSize;
   data->first_cluster = entry.firstClusterNo;
   file->data = data;

   // truncate at open
   if(file->type == FS_TYPE_FILE && (file->flags & FS_FLAG_TRUNCATE) && !(file->flags & FS_FLAG_READONLY) && data->file_size > 0) {
      if(fat_resize_file(file->filename, 0) < 0) {
         debug_printf("FS: failed to truncate %s\n", file->filename);
         free((uint32_t)file->data, sizeof(fs_file_data_t));
         file->data = NULL;
         return false;
      }
      data->file_size = 0;
   }

   return true;
}

static fs_file_t *fs_file_alloc(char *path, int flags) {
   fs_file_t *file = (fs_file_t*)malloc(sizeof(fs_file_t));
   if(!file) return NULL;
   file->window_index = -1;
   file->active = true;
   file->data = NULL;
   file->pipe = NULL;
   file->current_pos = 0;
   file->flags = flags;
   strcpy(file->filename, path);
   return file;
}

// file teardown
void fs_free(kobj_inst_t *inst) {
   fs_close(inst->data);
}

bool fs_connect(kobj_ref_t *ref, process_t *process) {
   (void)ref;
   (void)process;
   return true; // allow file inst to have multiple refs (via process copy)
}

// wrap opened file in a handle - closes on failure
static kobj_handle_t fs_file_handle(process_t *process, fs_file_t *file) {
   kobj_inst_t *inst = create_kobj_inst(KOBJ_FILE, file);
   if(!inst) {
      fs_close(file);
      return -1;
   }
   kobj_handle_t h = process_acquire_kobj(process, inst);
   if(h < 0) free_kobj_inst(inst); // fs_free closes file
   return h;
}

kobj_handle_t fs_open(process_t *process, char *path, int flags) {
   if(!path || strlen(path) == 0 || strlen(path) > 255)
      return -1;

   fs_file_t *file = fs_file_alloc(path, flags);
   if(!file)
      return -1;

   if(fs_is_term_path(path)) {
      fs_open_term(process, file);
      return fs_file_handle(process, file);
   }

   kmutex_lock(&fs_mutex);
   bool success = fs_open_locked(file);
   kmutex_unlock(&fs_mutex);
   if(!success) {
      fs_close(file);
      return -1;
   }
   return fs_file_handle(process, file);
}

bool fs_exists(char *path) {
   if(path == NULL || strlen(path) == 0 || strlen(path) > 255) return false;
   kmutex_lock(&fs_mutex);
   fat_dir_t entry;
   bool exists = fat_parse_path(path, true, &entry);
   kmutex_unlock(&fs_mutex);
   return exists;
}

void fs_close(fs_file_t *file) {
   if(file->data)
      free((uint32_t)file->data, sizeof(fs_file_data_t));
   if(file->pipe) {
      if(file->flags & FS_FLAG_WRITEONLY)
         file->pipe->writer_count--;
      if(file->pipe->read_waiting_task != -1) {
         task_state_t *task = &gettasks()[file->pipe->read_waiting_task];
         if(task->enabled && task->task_uid == file->pipe->read_waiting_uid) {
            task_resume(task);
            task->registers.ebx = FS_EOF;
         }
         file->pipe->read_waiting_task = -1;
      }
      if(file->pipe->write_waiting_task != -1) {
         task_state_t *task = &gettasks()[file->pipe->write_waiting_task];
         if(task->enabled && task->task_uid == file->pipe->write_waiting_uid) {
            task_resume(task);
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

static fs_dir_entry_t fs_get_dir_entry(fat_dir_t *item) {
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

static int fs_read_dir_locked(char *path, fs_dir_entry_t *entries, int max, int task) {
   uint16_t cluster = 0; // root
   if(!strequ(path, "/") && !strequ(path, "")) {
      fat_dir_t dir;
      if(!fat_parse_path(path, true, &dir))
         return -1; // not found
      if(!(dir.attributes & 0x10))
         return 0; // not a dir
      cluster = dir.firstClusterNo;
   }

   uint32_t bytes;
   fat_dir_t *items = fat_read_dir(cluster, &bytes);
   if(!items) {
      debug_printf("FS: failed reading directory '%s'\n", path);
      return -1;
   }

   int total = 0;
   int slots = bytes / sizeof(fat_dir_t);
   for(int i = 0; i < slots; i++) {
      if(items[i].filename[0] == 0) break; // end of directory
      if(items[i].filename[0] == 0xE5) continue; // deleted entry
      if(total < max) {
         fs_dir_entry_t entry = fs_get_dir_entry(&items[i]);
         if(copy_to_task(task, &entries[total], &entry, sizeof(entry)) != sizeof(entry)) {
            total = -1; // bad buffer
            break;
         }
      }
      total++;
   }

   free((uint32_t)items, bytes);
   return total;
}

// returns total entries in directory or -1 for error
int fs_read_dir(char *path, fs_dir_entry_t *entries, int max, int task) {
   if(path == NULL || strlen(path) > 255 || max < 0) return -1;
   kmutex_lock(&fs_mutex);
   int result = fs_read_dir_locked(path, entries, max, task);
   kmutex_unlock(&fs_mutex);
   return result;
}

bool fs_mkdir(char *path) {
   if(path == NULL || strlen(path) == 0 || strlen(path) > 255) {
      debug_printf("FS: invalid dir path\n");
      return false;
   }
   debug_printf("FS: creating new dir '%s'\n", path);
   kmutex_lock(&fs_mutex);
   bool success = fat_new_dir(path);
   kmutex_unlock(&fs_mutex);
   return success;
}

kobj_handle_t fs_new(process_t *process, char *path, int flags) {
   if(!path || strlen(path) == 0 || strlen(path) > 255) {
      debug_printf("fs_new: invalid file path\n");
      return -1;
   }

   fs_file_t *file = fs_file_alloc(path, flags);
   if(!file)
      return -1;

   kmutex_lock(&fs_mutex);
   bool success = fs_new_locked(file);
   kmutex_unlock(&fs_mutex);
   if(!success) {
      fs_close(file);
      return -1;
   }
   return fs_file_handle(process, file);
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

// task -1 for kernel, e.g. in task_write_to_window
int fs_write(fs_file_t *file, uint8_t *buffer, uint32_t size, int task) {
   if(file->type == FS_TYPE_TERM) {
      if(get_window_checked(file->window_index, file->window_uid)) {
         window_writestrn((char*)buffer, size, 0, file->window_index);
         return size;
      } else {
         debug_printf("FS: error writing to window %i\n", file->window_index);
         return FS_ERROR;
      }
   }
   
   if(file->type == FS_TYPE_FILE) {
      // write to file
      if(file->flags & FS_FLAG_READONLY) {
         debug_printf("FS: fs_write failed as %s was opened read only\n", file->filename);
         return FS_ERROR;
      }
      kmutex_lock(&fs_mutex);
      uint32_t pos = file->current_pos;
      if(file->flags & FS_FLAG_APPEND)
         pos = file->data->file_size;

      int written = fat_write_file_at(file->filename, buffer, pos, size, task);
      if(written > 0) {
         file->current_pos = pos + written;
         if(file->current_pos > file->data->file_size)
            file->data->file_size = file->current_pos;
      }
      kmutex_unlock(&fs_mutex);

      if(written < 0) {
         debug_printf("FS: error writing to file %s\n", file->filename);
         return FS_ERROR;
      }
      return written;
   }
   
   if(file->type == FS_TYPE_PIPE) {
      fs_pipe_t *pipe = file->pipe;
      if(!pipe) {
         debug_printf("api_write: pipe not active\n");
         return FS_ERROR;
      }

      if(pipe->size == FS_PIPE_BUF_SIZE) {
         if(task < 0)
            return FS_ERROR; // kernel writes can't wait, drop write
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

// notify window->read_task of read
void fs_read_window_callback(gui_window_t *window, bool eof) {
   char *user_buffer = window->read_buffer;
   int read_task = window->read_task;
   window->read_task = -1;

   task_state_t *task = &gettasks()[read_task];
   if(!task->enabled || task->task_uid != window->read_task_uid) {
      debug_printf("read: task %i not enabled or crashed\n", read_task);
      return;
   }
   if(!task->paused || task->pause_reason != PAUSE_READ) {
      debug_printf("read: task %i isn't waiting on read\n", read_task);
      return;
   }
   if(eof) {
      task->registers.ebx = FS_EOF;
   } else {
      int buf_size = window->text_index+1;
      char buffer[TEXT_BUFFER_LENGTH+1];
      strcpy_fixed(buffer, window->text_buffer, buf_size-1);
      buffer[buf_size-1] = '\0';
      int copy = buf_size;
      if(window->read_size < copy)
         copy = window->read_size;
      copy = copy_to_task(task->task_id, user_buffer, buffer, copy);

      task->registers.ebx = copy==buf_size?buf_size-1:copy;
   }
   task_resume(task);
   wm_event_defer_yield(task->task_id);
}

int fs_read(fs_file_t *file, void *buffer, size_t size, int task) {
   if(file->type == FS_TYPE_TERM) {
      gui_window_t *window = get_window_checked(file->window_index, file->window_uid);
      if(!window) {
         debug_printf("FS: error reading from window %i\n", file->window_index);
         return FS_ERROR;
      }

      if(file->flags & FS_FLAG_WRITEONLY) {
         debug_printf("FS: cannot read from write-only file %s\n", file->filename);
         return FS_ERROR;
      }
      // note: only one task can read from a windows stdin at a time as these get overwritten
      window->read_buffer = buffer;
      window->read_size = size;
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

   // read from file
   kmutex_lock(&fs_mutex);

   if(file->type == FS_TYPE_FILE) {
      if(file->flags & FS_FLAG_WRITEONLY) {
         debug_printf("FS: cannot read from write-only file %s\n", file->filename);
         kmutex_unlock(&fs_mutex);
         return FS_ERROR;
      }
      if(file->current_pos >= file->data->file_size) {
         kmutex_unlock(&fs_mutex);
         return FS_EOF;
      }
   } else {
      debug_printf("FS: cannot read from directory %s\n", file->filename);
      kmutex_unlock(&fs_mutex);
      return FS_ERROR;
   }

   if((int)size < 0)
      size = file->data->file_size;
   if(file->current_pos + size > file->data->file_size)
      size = file->data->file_size - file->current_pos;
   if(size == 0) {
      kmutex_unlock(&fs_mutex);
      return FS_EOF;
   }

   int read = fat_read_file_user(file->data->first_cluster, buffer, file->current_pos, size, task);
   int r = read;
   if(read >= 0) {
      file->current_pos += read;
   } else {
      debug_printf("FS: reading %s failed\n", file->filename);
      r = FS_ERROR;
   }
   
   kmutex_unlock(&fs_mutex);
   return r;
}

int fs_truncate(fs_file_t *file, int size) {
   if(file->type != FS_TYPE_FILE) {
      debug_printf("FS: cannot truncate non-file %s\n", file->filename);
      return FS_ERROR;
   }
   if(size < 0 || (uint32_t)size > fat_max_file_size()) {
      debug_printf("FS: invalid truncate size %i\n", size);
      return FS_ERROR;
   }
   if(file->flags & FS_FLAG_READONLY) {
      debug_printf("FS: cannot truncate read only file %s\n", file->filename);
      return FS_ERROR;
   }
   kmutex_lock(&fs_mutex);
   if(fat_resize_file(file->filename, (uint32_t)size) < 0) {
      debug_printf("FS: failed to truncate %s\n", file->filename);
      kmutex_unlock(&fs_mutex);
      return FS_ERROR;
   }
   file->data->file_size = size;
   if(file->current_pos > (uint32_t)size)
      file->current_pos = size;
   kmutex_unlock(&fs_mutex);
   return 0;
}

bool fs_unlink(char *path) {
   if(path == NULL || strlen(path) == 0 || strlen(path) > 255) return false;
   kmutex_lock(&fs_mutex);
   bool success = fat_delete_file(path);
   kmutex_unlock(&fs_mutex);
   return success;
}

bool fs_rmdir(char *path) {
   if(path == NULL || strlen(path) == 0 || strlen(path) > 255) return false;
   kmutex_lock(&fs_mutex);
   bool success = fat_delete_dir(path);
   kmutex_unlock(&fs_mutex);
   return success;
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
   kmutex_lock(&fs_mutex);
   bool success = fat_rename(oldpath, newname);
   kmutex_unlock(&fs_mutex);
   return success;
}

int fs_filesize(fs_file_t *file) {
   if(!file->data) return 0;
   return file->data->file_size;
}

int fs_filesize_path(char *path) {
   kmutex_lock(&fs_mutex);
   fat_dir_t entry;
   bool found = fat_parse_path(path, true, &entry);
   kmutex_unlock(&fs_mutex);
   if(!found) return -1;
   return entry.fileSize;
}

int fs_seek(fs_file_t *file, int offset, int type) {
   if(file->type != FS_TYPE_FILE || !file->data) {
      debug_printf("FS: cannot seek on non-file %s\n", file->filename);
      return -1;
   }

   int size = file->data->file_size;
   int base;
   if(type == SEEK_SET)
      base = 0;
   else if(type == SEEK_CUR)
      base = file->current_pos;
   else if(type == SEEK_END)
      base = size;
   else
      return -1;

   if(offset > 0 && base > 0x7FFFFFFF - offset)
      return -1; // would overflow

   int pos = base + offset;
   if(pos < 0)
      return -1; // can't seek before start of file
   if(pos > size)
      pos = size; // clamp to end

   file->current_pos = pos;
   return pos;
}

bool fs_create_pipe(process_t *process, kobj_handle_t *read_end, kobj_handle_t *write_end) {
   fs_pipe_t *pipe = (fs_pipe_t*)malloc(sizeof(fs_pipe_t));
   if(!pipe) return false;
   memset(pipe, 0, sizeof(fs_pipe_t));
   pipe->read_waiting_task = -1;
   pipe->write_waiting_task = -1;
   pipe->writer_count = 1;
   pipe->ref_count = 2;

   fs_file_t *read_file = fs_file_alloc("", 0);
   if(!read_file) {
      free((uint32_t)pipe, sizeof(fs_pipe_t));
      return false;
   }
   read_file->type = FS_TYPE_PIPE;
   read_file->pipe = pipe;

   fs_file_t *write_file = fs_file_alloc("", 0);
   if(!write_file) {
      free((uint32_t)pipe, sizeof(fs_pipe_t));
      free((uint32_t)read_file, sizeof(fs_file_t));
      return false;
   }
   write_file->type = FS_TYPE_PIPE;
   write_file->pipe = pipe;
   write_file->flags = FS_FLAG_WRITEONLY;

   kobj_handle_t read_h = fs_file_handle(process, read_file);
   if(read_h < 0) {
      fs_close(write_file);
      return false;
   }

   kobj_handle_t write_h = fs_file_handle(process, write_file);
   if(write_h < 0) {
      process_release_kobj(process, read_h);
      return false;
   }

   *read_end = read_h;
   *write_end = write_h;
   return true;
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
   task_resume(task);
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
   task_resume(task);
   pipe->write_waiting_task = -1;
   return true;
}
