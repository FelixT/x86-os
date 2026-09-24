#ifndef PAGING_H
#define PAGING_H

#include "stdbool.h"
#include "stdint.h"

// https://www.intel.com/content/dam/www/public/us/en/documents/manuals/64-ia-32-architectures-software-developer-vol-3a-part-1-manual.pdf
// -> Chapter 4
// https://wiki.osdev.org/Paging
// https://littleosbook.github.io/#paging

// mem types:
// 0 UC - uncacheable - WC not allowed
// 1 WC - write combining
// 4 WT - write-through
// 5 WP - write-protect
// 6 WB - write-back
// 7 UC- - uncached - can be overwritten by WC MTRR (PAT only)

// 4KiB paging

typedef struct page_dir_entry_t {
   uint32_t present    : 1; // 1 = the page is actually in the physical memory
   uint32_t rw         : 1; // 1 = page is r/w, 0 = read only
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
   uint32_t w_through  : 1; // pwt
   uint32_t no_cache   : 1; // pcd
   uint32_t accessed   : 1;
   uint32_t dirty      : 1;
   uint32_t pat        : 1; // used with pwt+pcd to select a PAT entry
   uint32_t global     : 1; // (unused)
   uint32_t available  : 3;
   uint32_t address    : 20; // physical address of start of 4KiB page
}__attribute__((packed, aligned(4))) page_table_entry_t;

void unmap(page_dir_entry_t *dir, uint32_t addr);
bool map(page_dir_entry_t *dir, uint32_t addr, uint32_t vaddr, int user, int rw, int no_cache);
int map_size(page_dir_entry_t *dir, uint32_t phys_addr, uint32_t virt_addr, uint32_t size, int user, int rw, int no_cache);
bool page_set_memtype(page_dir_entry_t *dir, uint32_t vaddr, uint32_t size, uint8_t pat_index);
void page_init();
uint32_t page_getphysical(page_dir_entry_t *dir, uint32_t vaddr);
int page_checkmapping(page_dir_entry_t *dir, uint32_t vaddr);
page_dir_entry_t *page_get_kernel_pagedir();
void swap_pagedir(page_dir_entry_t *dir);
page_dir_entry_t *new_page();
page_dir_entry_t *page_get_current();
void free_page_dir(page_dir_entry_t *dir);
void mtrr_print();

#define PAGE_SIZE 0x1000
#define PAGE_MASK (PAGE_SIZE - 1)

#define PAGE_USERRW 2
#define PAGE_USERREAD 1
#define PAGE_NOTPRESENT 0
#define PAGE_KERNELREAD -1
#define PAGE_KERNELRW -2

static inline uint32_t page_align_up(uint32_t addr) {
   return (addr + PAGE_MASK) & ~PAGE_MASK;
}

static inline uint32_t page_align_down(uint32_t addr) {
   return addr & ~PAGE_MASK;
}

#endif