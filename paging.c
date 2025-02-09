#include "paging.h"
#include "window.h"

#include "gui.h"
#include "memory.h"

// page directory:
// array of 1024 page dir entries

// kernel page directory
page_dir_entry_t *page_dir;

page_dir_entry_t *current_page_dir;

int debug = 0;

extern void load_page_dir(uint32_t *page_dir);
extern void page_enable();

void unmap(page_dir_entry_t *dir, uint32_t vaddr) {
   uint32_t index = vaddr / 0x1000; // 4096 bytes per page
   uint32_t dir_index = index / 1024; // 1024 table entries/dir
   uint32_t table_index = index%1024;

   if(dir[dir_index].present) {

      //page_dir[dir_index].present = 0;

      if(dir[dir_index].address) {

         page_table_entry_t *page_table = (page_table_entry_t*) (dir[dir_index].address << 12);

         page_table[table_index].present = 0;

      }

   }
}

void map(page_dir_entry_t *dir, uint32_t addr, uint32_t vaddr, int user, int rw) {

   // map 4 KiB aligned vadddr to 4 KiB aligned physical addr

   // get page
   uint32_t index = vaddr / 0x1000; // 4096 bytes per page
   uint32_t dir_index = index / 1024; // 1024 table entries/dir
   uint32_t table_index = index%1024;

   if(!dir[dir_index].present || !dir[dir_index].address) {
      page_table_entry_t *page_table = malloc(sizeof(page_table_entry_t) * 1024); // dir

      dir[dir_index].address = (uint32_t)page_table >> 12;

      // default everything to 0
      memset(page_table, 2, 1024*(int)sizeof(page_table_entry_t)); // set to 2 for rw = 1, else 0
   }

   page_table_entry_t *page_table = (page_table_entry_t*) (dir[dir_index].address << 12);

   page_table[table_index].present = 1;
   page_table[table_index].rw = rw;
   page_table[table_index].user = user;
   page_table[table_index].address = addr >> 12;
   //page_table[table_index].no_cache = 1;

   dir[dir_index].present = 1;
   dir[dir_index].rw = rw;
   dir[dir_index].user = user;
   //page_dir[dir_index].no_cache = 1;

}

extern uint32_t kernel_end;

page_dir_entry_t *new_page() {

   // we use malloc here as it currently satisfies out 4KiB alignment requirement

   page_dir_entry_t *dir = malloc(sizeof(page_dir_entry_t) * 1024);
   // memset 0
   uint8_t *entry = (uint8_t*)dir;
   for(int i = 0; i < 1024*(int)sizeof(page_dir_entry_t); i++)
      entry[i] = 2; // set to 2 for rw = 1, else 0

   // identity map kernel
   for(uint32_t i = KERNEL_START; i < KERNEL_END; i++)
      map(dir, i, i, 0, 0);

   // identity map stacks
   for(uint32_t i = STACKS_START; i < TOS_KERNEL; i++)
      map(dir, i, i, 1, 1); // user so progidle doesn't break upon push
      
   for(uint32_t i = TOS_KERNEL; i < TOS_PROGRAM; i++)
      map(dir, i, i, 1, 1);
   
   // identity map heap
   for(uint32_t i = HEAP_KERNEL; i < HEAP_KERNEL_END; i++)
      map(dir, i, i, 1, 1);

   // identity map framebuffer
   for(uint32_t i = (uint32_t)gui_get_framebuffer(); i < (uint32_t)gui_get_framebuffer()+gui_get_framebuffer_size(); i++)
      map(dir, i, i, 1, 1);

   return dir;

}

void page_init() {

   // set up kernel page and switch to it

   page_dir = new_page();
   swap_pagedir(page_dir);
   page_enable();

   window_writestr("\nPaging enabled\n", 0, 0);

}

uint32_t page_getphysical(page_dir_entry_t *dir, uint32_t vaddr) {
   uint32_t dir_index = vaddr >> 22;
   uint32_t table_index = vaddr >> 12 & 0x03FF;

   if(!dir[dir_index].present)
      return -1;

   page_table_entry_t *page_table = (page_table_entry_t *)(dir[dir_index].address << 12);

   if(!page_table[table_index].present)
      return -1;

   return (page_table[table_index].address << 12) + (vaddr & 0xFFF);
}

void page_enable_debug() {
   debug = 1;
}

page_dir_entry_t *page_get_kernel_pagedir() {
   return page_dir;
}

void swap_pagedir(page_dir_entry_t *dir) {
   current_page_dir = dir;
   load_page_dir((uint32_t*)&dir[0]);
}