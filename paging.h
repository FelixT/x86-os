#ifndef PAGING_H
#define PAGING_H

#include "stdbool.h"
#include "stdint.h"

// https://www.intel.com/content/dam/www/public/us/en/documents/manuals/64-ia-32-architectures-software-developer-vol-3a-part-1-manual.pdf
// -> Chapter 4
// https://wiki.osdev.org/Paging
// https://littleosbook.github.io/#paging

// 4KiB paging

typedef struct page_dir_entry_t {
   uint32_t present    : 1; // 1 = the page is actually in the physical memory
   uint32_t rw         : 1; // 1 = page is r/w, 0 = 
   uint32_t user       : 1; // 1 = all can access, 0 = only kernel
   uint32_t w_through  : 1; // 1 = write-through caching enabled, 0 = write-back caching enabled
   uint32_t no_cache   : 1; // 1 = not cached, 0 = cached
   uint32_t accessed   : 1; // 1 = pde or pte accessed during addr translation (unused)
   uint32_t available  : 1; // (unused/ignored)
   uint32_t page_size  : 1; // 1 = 4MiB, 0 = 4KiB (should always be 0)
   uint32_t global     : 1; // (unused)
   uint32_t available2 : 3; // (unused/ignored)
   uint32_t address    : 20; // bits 31:12 of physical addr of the page table that manages the 4MiB represented by this dir entry (note due to 4KiB aligment we only need these bits)
} __attribute__((packed, aligned(4))) page_dir_entry_t;

typedef struct page_table_entry_t {
   uint32_t present    : 1;
   uint32_t rw         : 1;
   uint32_t user       : 1;
   uint32_t w_through  : 1; // (unused)
   uint32_t no_cache   : 1; // (unused)
   uint32_t accessed   : 1;
   uint32_t dirty      : 1;
   uint32_t pat        : 2; // (unused)
   uint32_t available  : 3;
   uint32_t address    : 20; // physical address of start of 4KiB page
} __attribute__((packed, aligned(4))) page_table_entry_t;

void unmap(uint32_t addr);
void map(uint32_t addr, uint32_t vaddr, int user, int rw);
void page_init();
uint32_t page_getphysical(uint32_t vaddr);
void page_enable_debug();

#endif