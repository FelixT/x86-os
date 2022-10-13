#include "paging.h"

#include "gui.h"
#include "memory.h"

// map memory 1 to 1 for first 2 mb

// page directory:
// array of 1024 page dir entries

page_dir_entry_t *page_dir;

extern void load_page_dir(uint32_t *page_dir);
extern void page_enable();

void map(uint32_t addr, uint32_t vaddr, int user, int rw) {
   // get page
   uint32_t index = vaddr / 0x1000; // 4096 bytes per page
   uint32_t dir_index = index / 1024; // 1024 table entries/dir
   uint32_t table_index = index%1024;

   if(!page_dir[dir_index].present) {

      page_table_entry_t *page_table = malloc(sizeof(page_table_entry_t) * 1024);

      page_dir[dir_index].address = (uint32_t)page_table >> 12;

      uint8_t *entry = (uint8_t*)page_table;
      for(int i = 0; i < 1024*(int)sizeof(page_table_entry_t); i++)
         entry[0] = 0; // set to 2 for rw = 1, else 0
         
   } else {

      page_table_entry_t *page_table = (page_table_entry_t*) (page_dir[dir_index].address << 12);
      page_table[table_index].present = 1;
      page_table[table_index].rw = rw;
      page_table[table_index].user = user;
      page_table[table_index].address = addr >> 12;

   }

   page_dir[dir_index].present = 1;
   page_dir[dir_index].rw = rw;
   page_dir[dir_index].user = user;

}

extern uint32_t kernel_end;

void page_init() {
   // we use malloc here as it currently satisfies out 4KiB alignment requirement

   // init
   page_dir = malloc(sizeof(page_dir_entry_t) * 1024);
   // memset 0
   uint8_t *entry = (uint8_t*)page_dir;
   for(int i = 0; i < 1024*(int)sizeof(page_dir_entry_t); i++)
      entry[i] = 0; // set to 2 for rw = 1, else 0

   // identity map kernel
   for(uint32_t i = KERNEL_START; i < (uint32_t)&kernel_end; i++)
      map(i, i, 0, 1);

   // identity map stacks
   for(uint32_t i = STACKS_START; i < TOS_KERNEL; i++)
      map(i, i, 1, 1); // user so prog1 doesn't break upon push
      
   for(uint32_t i = TOS_KERNEL; i < TOS_PROGRAM; i++)
      map(i, i, 1, 1);
   
   // identity map heap
   for(uint32_t i = HEAP_KERNEL; i < HEAP_KERNEL_END; i++)
      map(i, i, 1, 1);

   // identity map framebuffer
   for(uint32_t i = (uint32_t)gui_get_framebuffer(); i < (uint32_t)gui_get_framebuffer()+gui_get_framebuffer_size(); i++)
      map(i, i, 1, 1);

   load_page_dir((uint32_t*)&page_dir[0]);
   page_enable();

   gui_window_writestr("\nPaging enabled\n", 0, 0);

}

uint32_t page_getphysical(uint32_t vaddr) {
   uint32_t dir_index = vaddr >> 22;
   uint32_t table_index = vaddr >> 12 & 0x03FF;

   if(!page_dir[dir_index].present)
      return -1;

   page_table_entry_t *page_table = (page_table_entry_t *)(page_dir[dir_index].address << 12);

   if(!page_table[table_index].present)
      return -1;

   return (page_table[table_index].address << 12) + (vaddr & 0xFFF);
}