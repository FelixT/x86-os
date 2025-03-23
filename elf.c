// elf32
// https://refspecs.linuxbase.org/elf/gabi4+/ch5.pheader.html

#include "elf.h"
#include "paging.h"
#include "windowmgr.h"
#include "memory.h"
#include "tasks.h"

void elf_run(registers_t *regs, uint8_t *prog, int argc, char **args) {

   //page_enable_debug();
   debug_writestr("elf_run called with prog at ");
   debug_writehex((uint32_t)prog);
   debug_writestr("\n");

   elf_header_t *elf_header = (elf_header_t*)prog;
   elf_prog_header_t *prog_header = (elf_prog_header_t*)(prog + elf_header->prog_header);
   // fail if
   // elf_header->type != 2 i.e. not exec
   // elf_header->prog_header_entry_count == 0
   // elf signature invalid
   if(elf_header->type != 2) {
      debug_writestr("\nELF Type ");
      debug_writeuint(elf_header->type);
      debug_writestr(" is unsupported\n");
      return;
   }

   // Work out how much memory to assign

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

   uint32_t vmem_size = vmem_end - vmem_start;

   // create page directory
   page_dir_entry_t *dir = new_page();

   // allocate memory
   uint8_t *newProg = malloc(vmem_size);
   // fill with 0s
   for(int i = 0; i < (int)vmem_size; i++)
      newProg[i] = 0;

   // start of newProg maps to vmem_start  

   prog_header = (elf_prog_header_t*)(prog + elf_header->prog_header);

   debug_writestr("Mapping ");
   debug_writehex((uint32_t)newProg);
   debug_writestr(" - ");
   debug_writehex((uint32_t)newProg + vmem_size);
   debug_writestr(" to ");
   debug_writehex(vmem_start);
   debug_writestr(" - ");
   debug_writehex(vmem_end);
   debug_writestr("\n");

   // map vmem
   for(uint32_t i = 0; i < vmem_size; i++)
      map(dir, (uint32_t)newProg + i, vmem_start + i, 1, 1);

   debug_writestr("Start: ");
   debug_writehex(page_getphysical(dir, elf_header->entry));
   debug_writestr("\n");

   // copy program to new location and assign virtual memory for each segment
   for(int i = 0; i < elf_header->prog_header_entry_count; i++) {

      if(prog_header->segment_type != 1) {
         // if not LOAD
         //continue;
      }

      uint32_t file_offset = prog_header->p_offset;
      uint32_t vmem_offset = prog_header->p_vaddr - vmem_start;

      // copy
      for(int i = 0; i < (int)prog_header->p_filesz; i++)
         newProg[vmem_offset + i] = prog[file_offset + i];

      //int rw = (prog_header->flags & 0x1) == 0x1;
      
      prog_header++;
   }

   int task_index = get_free_task_index();

   create_task_entry(task_index, elf_header->entry, (vmem_end - vmem_start), false);
   gettasks()[task_index].vmem_start = vmem_start;
   gettasks()[task_index].vmem_end = vmem_end;
   gettasks()[task_index].prog_start = (uint32_t)newProg;
   gettasks()[task_index].page_dir = dir;
   launch_task(task_index, regs, true);

   // push args
   regs->useresp -= 4;
   ((uint32_t*)regs->useresp)[0] = (uint32_t)args; // char **args
   regs->useresp -= 4;
   ((uint32_t*)regs->useresp)[0] = argc; // int argc
   regs->useresp -= 4;
   ((uint32_t*)regs->useresp)[0] = vmem_start; // push dummy (mapped) return addr

   debug_writestr("\nStarting at: ");
   debug_writehex(elf_header->entry);
   debug_writestr("\n");

}