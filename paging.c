#include "paging.h"
#include "window.h"

#include "gui.h"
#include "windowmgr.h"
#include "memory.h"
#include "cpu.h"

// page directory:
// array of 1024 page dir entries

// kernel page directory
page_dir_entry_t *page_dir;

page_dir_entry_t *current_page_dir;

extern void load_page_dir(uint8_t *page_dir);
extern void page_enable();

void unmap(page_dir_entry_t *dir, uint32_t vaddr) {
   uint32_t index = vaddr / 0x1000; // 4096 bytes per page
   uint32_t dir_index = index / 1024; // 1024 table entries/dir
   uint32_t table_index = index%1024;

   if(dir[dir_index].present) {

      //page_dir[dir_index].present = 0;

      if(dir[dir_index].address) {

         page_table_entry_t *page_table = (page_table_entry_t*) ((dir[dir_index].address << 12) + V_KERNEL_OFFSET);

         page_table[table_index].present = 0;

      }

   }

   if(dir == current_page_dir)
      invlpg(vaddr);
}

bool map(page_dir_entry_t *dir, uint32_t addr, uint32_t vaddr, int user, int rw, int no_cache) {

   // map 4 KiB aligned vadddr to 4 KiB aligned physical addr
   if(!dir) return false;

   // get page
   uint32_t index = vaddr / 0x1000; // 4096 bytes per page
   uint32_t dir_index = index / 1024; // 1024 table entries/dir
   uint32_t table_index = index%1024;

   if(!dir[dir_index].present || !dir[dir_index].address) {
      page_table_entry_t *page_table = malloc(sizeof(page_table_entry_t) * 1024); // dir
      if(!page_table) {
         debug_printf("map: out of physical memory creating page table vaddr 0x%h\n", vaddr);
         return false;
      }

      // default everything to 0
      memset(page_table, 0, 1024*(int)sizeof(page_table_entry_t)); // set to 2 for rw = 1, else 0

      dir[dir_index].address = ((uint32_t)page_table - V_KERNEL_OFFSET) >> 12;
      dir[dir_index].present = 1;
      dir[dir_index].rw = 1;
      dir[dir_index].user = 1;
   }

   page_table_entry_t *page_table = (page_table_entry_t*) ((dir[dir_index].address << 12) + V_KERNEL_OFFSET);

   bool was_present = page_table[table_index].present == 1;
   page_table[table_index].present = 1;
   page_table[table_index].rw = rw;
   page_table[table_index].user = user;
   page_table[table_index].address = addr >> 12;
   page_table[table_index].no_cache = no_cache;

   if(was_present && dir == current_page_dir)
      invlpg(vaddr);

   return true;
}

int map_size(page_dir_entry_t *dir, uint32_t phys_addr, uint32_t virt_addr, uint32_t size, int user, int rw, int no_cache) {
   uint32_t phys_start = page_align_down(phys_addr);
   uint32_t virt_start = page_align_down(virt_addr);

   uint32_t phys_end = page_align_up(phys_addr + size);

   int mapped = 0;
   for(uint32_t paddr = phys_start, vaddr = virt_start; paddr < phys_end; paddr += PAGE_SIZE, vaddr += PAGE_SIZE) {
      if(!map(dir, paddr, vaddr, user, rw, no_cache))
         break;
      mapped++;
   }

   return mapped;
}

extern uint32_t kernel_end;

page_dir_entry_t *new_page() {

   // we use malloc here as it satisfies out 4KiB alignment requirement

   page_dir_entry_t *dir = malloc(sizeof(page_dir_entry_t) * 1024);
   if(!dir) {
      debug_printf("new_page: out of physical memory for page directory\n");
      return NULL;
   }
   // memset 0
   uint8_t *entry = (uint8_t*)dir;
   memset(entry, 0, 1024*(int)sizeof(page_dir_entry_t));

   bool ok = true;

   // identity map kernel binary
   for(uint32_t i = KERNEL_START/0x1000; i < KERNEL_END/0x1000; i++)
      ok &= map(dir, i*0x1000, i*0x1000, 0, 0, 0);

   // identity map heap for kernel
   for(uint32_t i = HEAP_KERNEL/0x1000; i < HEAP_KERNEL_END/0x1000; i++)
      ok &= map(dir, i*0x1000, i*0x1000, 0, 0, 0);

   // identity map framebuffer
   for(uint32_t i = (uint32_t)gui_get_framebuffer()/0x1000; i < ((uint32_t)gui_get_framebuffer()+gui_get_framebuffer_size()+0xFFF)/0x1000; i++)
      ok &= map(dir, i*0x1000, i*0x1000, 0, 0, 0);
   page_set_memtype(dir, (uint32_t)gui_get_framebuffer(), gui_get_framebuffer_size(), 1); // use write combining

   if(!ok) {
      debug_printf("new_page: incomplete kernel mapping, discarding page dir\n");
      free_page_dir(dir);
      return NULL;
   }

   return dir;
}

bool pat_enabled = false;

// update memory type for already mapped memory (PAT)
bool page_set_memtype(page_dir_entry_t *dir, uint32_t vaddr, uint32_t size, uint8_t pat_index) {
   if(!pat_enabled) return false;

   int pages = (size + 0xFFF) / 0x1000;
   for(int i = 0; i < pages; i++) {
      uint32_t index = (vaddr + i*0x1000) / 0x1000;
      uint32_t dir_index = index / 1024;
      uint32_t table_index = index%1024;

      if(!dir[dir_index].present || !dir[dir_index].address)
         return false;

      page_table_entry_t *page_table = (page_table_entry_t *)((dir[dir_index].address << 12) + V_KERNEL_OFFSET);
      page_table[table_index].w_through = pat_index&0x1; // pwt
      page_table[table_index].no_cache = (pat_index>>1)&0x1;
      page_table[table_index].pat = (pat_index>>2)&0x1;
   }

   return true;
}

typedef struct mtrr_t {
   uint8_t index;
   uint8_t type;
   uint64_t base_addr;
   uint64_t region_mask;
   uint64_t size;
} mtrr_t;

mtrr_t mtrrs[11];
int mtrr_count = 0;

// set up PAT + read MTRRs set up by BIOS
void mem_init() {
   // fetch cpu info
   char cpu_str[13];
   get_cpu_str(cpu_str);
   cpu_str[12] = '\0';

   uint32_t a, b, c, d;
   cpuid(1, &a, &b, &c, &d);
   uint8_t stepping_id = a&0xF; // revision
   uint8_t model = (a>>4)&0xF;
   uint8_t family_id = (a>>8)&0xF;
   uint8_t type = (a>>12)&0x3; // processor type
   // check MTRR/PAT support
   bool mtrr = (d>>12)&0x1;
   pat_enabled = (d>>16)&0x1;

   debug_printf("CPU %s model %u family %u type %u rev %u\n", cpu_str, model, family_id, type, stepping_id);

   if(pat_enabled) {      
      // set up write combining
      // remap PAT1 to WC

      // read IA32_PAT
      uint64_t pat = rdmsr(MSR_PAT);
      // set pat1 (wt/4) to wc/1
      uint8_t pat0 = (uint8_t)rdmsr(MSR_PAT);
      pat = (((pat>>16)<<16)|(1<<8))|pat0;
      wrmsr(MSR_PAT, pat);

      // invalidate cache
      wbinvd();
   } else {
      debug_printf("PAT disabled\n");
   }

   if(mtrr) {
      // read IA32_MTRRCAP MSR
      uint64_t mtrr_cap = rdmsr(MSR_MTRR_CAP);
      int vcnt = mtrr_cap&0xFF; // number of variable range registers
      bool wc = (mtrr_cap>>10)&0x1; // write combining enabled
      
      // read IA32_MTRR_DEF_TYPE - default properties of regions not encompassed by MTRRs
      uint64_t mtrr_def_type = rdmsr(MSR_MTRR_DEF_TYPE);
      uint8_t def_type = (uint8_t)mtrr_def_type;
      bool e = (mtrr_def_type>>11)&0x1; // MTRRs enabled

      debug_printf("MTRR enabled (%i) with default type %u - wc %i\n", vcnt, def_type, wc);

      int address_width = 32;
      uint32_t max_leaf = get_cpuid_ext_max();
      if(max_leaf >= 0x80000008) {
         cpuid(0x80000008, &a, &b, &c, &d);
         address_width = a&0xFF;
      }

      if(e && vcnt > 0) {
         // MTRR enabled & variable range MTRRs exist
         // read into mtrrs array (and detect any overlapping with framebuffer)
         uint64_t fb_start = (uint32_t)gui_get_framebuffer();
         uint64_t fb_size = gui_get_framebuffer_size();

         for(int i = 0; i < vcnt; i++) {       
            uint64_t mtrr_phys_base = rdmsr(MSR_MTRR_PHYS_BASE + 2*i);
            uint8_t mem_type = (uint8_t)mtrr_phys_base;
            uint64_t mask = (((uint64_t)1<<(39+1))-1)<<12; // AND mask for 51:12
            uint64_t base_addr = mtrr_phys_base&mask;
            if(base_addr > 0xFFFFFFFF) continue; // outside x86 mem range

            uint64_t mtrr_phys_mask = rdmsr(MSR_MTRR_PHYS_MASK + 2*i);
            bool valid = (mtrr_phys_mask>>11)&0x1;
            if(!valid) continue;

            uint64_t region_mask = mtrr_phys_mask&mask;
            uint64_t size = (~region_mask+1)&(((uint64_t)1<<address_width)-1);

            mtrrs[mtrr_count].index = i;
            mtrrs[mtrr_count].type = mem_type;
            mtrrs[mtrr_count].base_addr = base_addr;
            mtrrs[mtrr_count].region_mask = region_mask;
            mtrrs[mtrr_count].size = size;
            mtrr_count++;

            if(fb_start + fb_size > base_addr && fb_start < base_addr + size)
               debug_printf("MTRR %i type %u overlaps with framebuffer\n", i, mem_type);

            if(mtrr_count >= 11) break;
         }
      }
   }
}

void mtrr_print() {
   // print MTRRs set by BIOS (for debugging)
   for(int i = 0; i < mtrr_count; i++) {
      uint32_t start = (uint32_t)mtrrs[i].base_addr;
      uint32_t end = (uint32_t)(mtrrs[i].base_addr + mtrrs[i].size);
      debug_printf("%i: 0x%h - 0x%h type %u\n", mtrrs[i].index, start, end, mtrrs[i].type);
   }
}

void page_init() {

   mem_init();

   // set up kernel page and switch to it
   page_dir = new_page();

   // double fault tss uses kernel page
   extern tss_t df_tss_start;
   df_tss_start.cr3 = (uint32_t)((uint8_t*)page_dir - V_KERNEL_OFFSET);

   // map kernel stack
   for(uint32_t i = KSTACK_START/0x1000; i < TOS_KERNEL/0x1000; i++)
      map(page_dir, i*0x1000, i*0x1000, 0, 0, 0);
   // map double fault stack
   for(uint32_t i = KSTACK_DF_START/0x1000; i < KSTACK_DF_TOS/0x1000; i++)
      map(page_dir, i*0x1000, i*0x1000, 0, 0, 0);

   // this page is used for the idle process which needs heap to not crash
   for(uint32_t i = HEAP_KERNEL/0x1000; i < HEAP_KERNEL_END/0x1000; i++)
      map(page_dir, i*0x1000, i*0x1000, 1, 1, 0);

   swap_pagedir(page_dir);

   page_enable();

   window_writestr("Paging enabled\n", 0, 0);

}

uint32_t page_getphysical(page_dir_entry_t *dir, uint32_t vaddr) {
   uint32_t dir_index = vaddr >> 22;
   uint32_t table_index = vaddr >> 12 & 0x03FF;

   if(!dir[dir_index].present)
      return -1;

   page_table_entry_t *page_table = (page_table_entry_t *)((dir[dir_index].address << 12) + V_KERNEL_OFFSET);

   if(!page_table[table_index].present)
      return -1;

   return (page_table[table_index].address << 12) + (vaddr & 0xFFF);
}

int page_checkmapping(page_dir_entry_t *dir, uint32_t vaddr) {
   uint32_t dir_index = vaddr >> 22;
   uint32_t table_index = vaddr >> 12 & 0x03FF;

   if(!dir[dir_index].present)
      return PAGE_NOTPRESENT;

   page_table_entry_t *page_table = (page_table_entry_t *)((dir[dir_index].address << 12) + V_KERNEL_OFFSET);

   if(!page_table[table_index].present)
      return PAGE_NOTPRESENT;

   if(!page_table[table_index].user)
      return page_table[table_index].rw ? PAGE_KERNELRW : PAGE_KERNELREAD;
   else
      return page_table[table_index].rw ? PAGE_USERRW : PAGE_USERREAD;
}

page_dir_entry_t *page_get_kernel_pagedir() {
   return page_dir;
}

void swap_pagedir(page_dir_entry_t *dir) {
   current_page_dir = dir;
   load_page_dir((uint8_t*)&dir[0] - V_KERNEL_OFFSET);
}

page_dir_entry_t *page_get_current() {
   return current_page_dir;
}

void free_page_dir(page_dir_entry_t *dir) {
   // free page tables
   for(int i = 0; i < 1024; i++) {
      if(dir[i].present && dir[i].address) {
         free((dir[i].address << 12) + V_KERNEL_OFFSET, sizeof(page_table_entry_t)*1024);
      }
   }
   // free page dir
   free((uint32_t)dir, sizeof(page_dir_entry_t)*1024);
}