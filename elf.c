// elf32

#include "gui.h"
#include "paging.h"
#include <stdint.h>

// https://wiki.osdev.org/ELF#Header
typedef struct elf_header_t {
   uint8_t magic[4];
   uint8_t bits; // 1=32, 2=64
   uint8_t endian;
   uint8_t header_version; // elf header version
   uint8_t os_abi;
   uint8_t padding[8];

   uint16_t type; // 1 = relocatable, 2 = executable, 3 = shared, 4 = core 
   uint16_t instruction_set;
   uint32_t version; // elf version
   uint32_t entry;
   uint32_t prog_header; // position of program header
   uint32_t section_header;
   uint32_t flags;
   uint16_t header_size;
   uint16_t prog_header_entry_size;
   uint16_t prog_header_entry_count;
   uint16_t section_header_entry_size;
   uint16_t section_header_entry_count;
   uint16_t section_header_names_entry;
} __attribute__((packed)) elf_header_t;

typedef struct elf_prog_header_t {
   uint32_t segment_type;
   uint32_t p_offset; // The offset in the file that the data for this segment can be found 
   uint32_t p_vaddr; // Where you should start to put this segment in virtual memory (p_vaddr)
   uint32_t p_paddr; // unusused physical load address
   uint32_t p_filesz; // Size of the segment in the file 
   uint32_t p_memsz; // Size of the segment in memory 
   uint32_t flags;
   uint32_t alignment; // required alignment

} __attribute__((packed)) elf_prog_header_t;

void elf_run(void *regs, uint8_t *prog, int index) {

   elf_header_t *elf_header = (elf_header_t*)prog;
   elf_prog_header_t *prog_header = (elf_prog_header_t*)(prog + elf_header->prog_header);

   gui_window_writestr("Entry: ", 0, 0);
   gui_window_writeuint(elf_header->entry, 0, 0);

   gui_window_writestr("\nType: ", 0, 0);
   gui_window_writeuint(elf_header->type, 0, 0);

   gui_window_writestr("\nCount: ", 0, 0);
   gui_window_writeuint(elf_header->prog_header_entry_count, 0, 0);

   if(elf_header->type != 2) {
      gui_window_writestr("\nELF Type ", 0, 0);
      gui_window_writeuint(elf_header->type, 0, 0);
      gui_window_writestr(" is unsupported\n", 0, 0);
      return;
   }

   // fail if
   // elf_header->type != 2 i.e. not exec
   // elf_header->prog_header_entry_count == 0
   // elf signature invalid

   // get virtual memory start addr, end address
   uint32_t vmem_start = elf_header->entry; // address in virtual memory where program is loaded in
   uint32_t vmem_end = vmem_start;

   for(int i = 0; i < elf_header->prog_header_entry_count; i++) {
      if(prog_header->p_vaddr < vmem_start)
         vmem_start = prog_header->p_vaddr;

      if(prog_header->p_vaddr + prog_header->p_memsz > vmem_end)
         vmem_end = prog_header->p_vaddr + prog_header->p_memsz;

      prog_header++;
   }

   gui_window_writestr("\nVmem Start: ", 0, 0);
   gui_window_writeuint(vmem_start, 0, 0);

   gui_window_writestr("\nVmem End: ", 0, 0);
   gui_window_writeuint(vmem_end, 0, 0);

   // allocate memory
   uint8_t *newProg = malloc(vmem_end - vmem_start);
   // fill with 0s
   for(int i = 0; i < (int)(vmem_end - vmem_start); i++)
      newProg[i] = 0;

   // start of newProg maps to vmem_start  

   prog_header = (elf_prog_header_t*)(prog + elf_header->prog_header);

   // copy program to new location and assign virtual memory for each segment
   for(int i = 0; i < elf_header->prog_header_entry_count; i++) {

      gui_window_writestr("\nSegment ", 0, 0);
      gui_window_writeuint(i, 0, 0);

      gui_window_writestr("\nType ", 0, 0);
      gui_window_writeuint(prog_header->segment_type, 0, 0);

      if(prog_header->segment_type != 1) {
         // if not LOAD
         //continue;
      }

      uint32_t file_offset = prog_header->p_offset;
      uint32_t vmem_offset = prog_header->p_vaddr - vmem_start;

      gui_window_writestr("\nFile offset ", 0, 0);
      gui_window_writeuint(file_offset, 0, 0);
      gui_window_writestr("\nVmem offset ", 0, 0);
      gui_window_writeuint(vmem_offset, 0, 0);

      // copy
      for(int i = 0; i < (int)prog_header->p_filesz; i++)
         newProg[vmem_offset + i] = prog[file_offset + i];

      //int rw = (prog_header->flags & 0x1) == 0x1;

      // map vmem
      for(int i = 0; i < (int)prog_header->p_memsz; i++)
         map((uint32_t)newProg + vmem_offset + i, prog_header->p_vaddr + i, 1, 1);

      //gui_window_writestr("\nVirtual addr: ", 0, 0);
      //gui_window_writeuint(prog_header->p_vaddr, 0, 0);

      //gui_window_writestr("\nAlignment: ", 0, 0);
      //gui_window_writeuint(prog_header->alignment, 0, 0);

      gui_window_writestr("\nMapping ", 0, 0);
      gui_window_writeuint((uint32_t)newProg + vmem_offset, 0, 0);
      gui_window_writestr(" to ", 0, 0);
      gui_window_writeuint(prog_header->p_vaddr, 0, 0);
      gui_window_writestr(" with size ", 0, 0);
      gui_window_writeuint((uint32_t)prog_header->p_memsz, 0, 0);
      gui_window_writestr("\n", 0, 0);
      
      prog_header++;
   }

   uint32_t offset = elf_header->entry - vmem_start;

   gui_window_writestr("\nOffset: ", 0, 0);
   gui_window_writeuint(offset, 0, 0);

   //gui_draw();
   //while(true);


   create_task_entry(index, elf_header->entry, vmem_end - vmem_start, false);
   launch_task(index, regs, true);
   gettasks()[index].vmem_start = vmem_start;
   gettasks()[index].vmem_end = vmem_end;
   gettasks()[index].prog_start = (uint32_t)newProg;

   gui_window_writestr("\nStarting at: ", 0, 0);
   gui_window_writeuint(elf_header->entry, 0, 0);
   gui_window_writestr("\n", 0, 0);

}