#include "paging.h"
#include "window.h"

#include "gui.h"
#include "windowmgr.h"
#include "memory.h"

// page directory:
// array of 1024 page dir entries

// kernel page directory
page_dir_entry_t *page_dir;

page_dir_entry_t *current_page_dir;

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
      dir[dir_index].present = 1;
      dir[dir_index].rw = 1;
      dir[dir_index].user = 1;
      //page_dir[dir_index].no_cache = 1;

      // default everything to 0
      memset(page_table, 0, 1024*(int)sizeof(page_table_entry_t)); // set to 2 for rw = 1, else 0
   }

   page_table_entry_t *page_table = (page_table_entry_t*) (dir[dir_index].address << 12);

   page_table[table_index].present = 1;
   page_table[table_index].rw = rw;
   page_table[table_index].user = user;
   page_table[table_index].address = addr >> 12;
   //page_table[table_index].no_cache = 1;

}

int map_size(page_dir_entry_t *dir, uint32_t phys_addr, uint32_t virt_addr, uint32_t size, int user, int rw) {
   uint32_t phys_start = page_align_down(phys_addr);
   uint32_t virt_start = page_align_down(virt_addr);

   uint32_t phys_end = page_align_up(phys_addr + size);

   int mapped = 0;
   for(uint32_t paddr = phys_start, vaddr = virt_start; paddr < phys_end; paddr += PAGE_SIZE, vaddr += PAGE_SIZE) {
      map(dir, paddr, vaddr, user, rw);
      mapped++;
   }

   return mapped;
}

extern uint32_t kernel_end;

void debug_mapping(page_dir_entry_t *dir, uint32_t vaddr) {
    uint32_t index = vaddr / 0x1000;
    uint32_t dir_index = index / 1024;
    uint32_t table_index = index % 1024;
    
   debug_printf("Virtual addr: 0x%h\n", vaddr);
   debug_printf("Dir index: %i, Table index: %i\n", dir_index, table_index);
   debug_printf("PDE: present=%i, rw=%i, user=%i, addr=0x%h\n", 
           dir[dir_index].present, dir[dir_index].rw, 
           dir[dir_index].user, dir[dir_index].address);
    
    if (dir[dir_index].present) {
        page_table_entry_t *page_table = (page_table_entry_t*)(dir[dir_index].address << 12);
        debug_printf("PTE: present=%i, rw=%i, user=%i, addr=0xhx\n",
               page_table[table_index].present, page_table[table_index].rw,
               page_table[table_index].user, page_table[table_index].address);
    }
}

page_dir_entry_t *new_page() {

   // we use malloc here as it satisfies out 4KiB alignment requirement

   page_dir_entry_t *dir = malloc(sizeof(page_dir_entry_t) * 1024);
   // memset 0
   uint8_t *entry = (uint8_t*)dir;
   memset(entry, 0, 1024*(int)sizeof(page_dir_entry_t));

   // identity map kernel binary (already loaded into higher half)
   for(uint32_t i = KERNEL_START/0x1000; i <= KERNEL_END/0x1000; i++)
      map(dir, i*0x1000, i*0x1000, 0, 0);
   // identity map program stack
   for(uint32_t i = STACKS_START/0x1000; i < TOS_PROGRAM/0x1000; i++)
      map(dir, i*0x1000, i*0x1000, 1, 1);

   // map kernel stack
   uint32_t v_offset = (V_KSTACK_START - KSTACK_START)/0x1000;
   for(uint32_t i = KSTACK_START/0x1000; i < TOS_KERNEL/0x1000; i++)
      map(dir, i*0x1000, (v_offset+i)*0x1000, 0, 0);

   // identity map heap for kernel
   for(uint32_t i = HEAP_KERNEL/0x1000; i < HEAP_KERNEL_END/0x1000; i++)
      map(dir, i*0x1000, i*0x1000, 0, 0);

   // map heap to kernel
   v_offset = (V_HEAP_KERNEL - HEAP_KERNEL)/0x1000;
   for(uint32_t i = HEAP_KERNEL/0x1000; i < HEAP_KERNEL_END/0x1000; i++)
      map(dir, i*0x1000, (v_offset+i)*0x1000, 0, 0);
   // identity map framebuffer
   for(uint32_t i = (uint32_t)gui_get_framebuffer()/0x1000; i < ((uint32_t)gui_get_framebuffer()+gui_get_framebuffer_size()+0xFFF)/0x1000; i++)
      map(dir, i*0x1000, i*0x1000, 0, 0);

   return dir;
}

uint32_t get_esp() {
    return (uint32_t)__builtin_frame_address(0);
}

void page_init() {

   // set up kernel page and switch to it

   page_dir = new_page();

   // this page is used for the idle process which needs heap to not crash
   for(uint32_t i = HEAP_KERNEL/0x1000; i < HEAP_KERNEL_END/0x1000; i++)
      map(page_dir, i*0x1000, i*0x1000, 1, 1);

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

page_dir_entry_t *page_get_kernel_pagedir() {
   return page_dir;
}

void swap_pagedir(page_dir_entry_t *dir) {
   current_page_dir = dir;
   load_page_dir((uint32_t*)&dir[0] - (V_KERNEL_START - KERNEL_START));
}

page_dir_entry_t *page_get_current() {
   return current_page_dir;
}