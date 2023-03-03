// elf32

#include "elf.h"
#include "gui.h"
#include "paging.h"

void elf_run(registers_t *regs, uint8_t *prog, int index, int argc, char **args) {

   //page_enable_debug();

   elf_header_t *elf_header = (elf_header_t*)prog;
   elf_prog_header_t *prog_header = (elf_prog_header_t*)(prog + elf_header->prog_header);

   /*gui_window_writestr("Entry: ", 0, 0);
   gui_window_writeuint(elf_header->entry, 0, 0);

   gui_window_writestr("\nType: ", 0, 0);
   gui_window_writeuint(elf_header->type, 0, 0);

   gui_window_writestr("\nCount: ", 0, 0);
   gui_window_writeuint(elf_header->prog_header_entry_count, 0, 0);*/

   if(elf_header->type != 2) {
      gui_writestr("\nELF Type ", 0);
      gui_writeuint(elf_header->type, 0);
      gui_writestr(" is unsupported\n", 0);
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

   uint32_t vmem_size = vmem_end - vmem_start;

   /*gui_window_writestr("\nVmem Start: ", 0, 0);
   gui_window_writeuint(vmem_start, 0, 0);

   gui_window_writestr("\nVmem End: ", 0, 0);
   gui_window_writeuint(vmem_end, 0, 0);*/

   // allocate memory
   uint8_t *newProg = malloc(vmem_end - vmem_start);
   // fill with 0s
   for(int i = 0; i < (int)(vmem_end - vmem_start); i++)
      newProg[i] = 0;

   // start of newProg maps to vmem_start  

   prog_header = (elf_prog_header_t*)(prog + elf_header->prog_header);

   gui_writestr("Mapping ", 0);
   gui_writeuint((uint32_t)newProg, 0);
   gui_writestr(" - ", 0);
   gui_writeuint((uint32_t)newProg + vmem_size, 0);

   gui_writestr(" to ", 0);

   gui_writeuint(vmem_start, 0);
   gui_writestr(" - ", 0);
   gui_writeuint(vmem_end, 0);
   gui_writestr("\n", 0);

   // map vmem
   for(uint32_t i = 0; i < vmem_size; i++)
      map((uint32_t)newProg + i, vmem_start + i, 1, 1);

   // copy program to new location and assign virtual memory for each segment
   for(int i = 0; i < elf_header->prog_header_entry_count; i++) {

      gui_writestr("\nSegment ", 0);
      gui_writeuint(i, 0);

      gui_writestr(" Type ", 0);
      gui_writeuint(prog_header->segment_type, 0);

      if(prog_header->segment_type != 1) {
         // if not LOAD
         //continue;
      }

      uint32_t file_offset = prog_header->p_offset;
      uint32_t vmem_offset = prog_header->p_vaddr - vmem_start;

      gui_writestr("\nFile offset ", 0);
      gui_writeuint(file_offset, 0);
      gui_writestr(" size ", 0);
      gui_writeuint((uint32_t)prog_header->p_filesz, 0);
      gui_writestr(" Vmem offset ", 0);
      gui_writeuint(vmem_offset, 0);
      gui_writestr(" size ", 0);
      gui_writeuint((uint32_t)prog_header->p_memsz, 0);

      // copy
      for(int i = 0; i < (int)prog_header->p_filesz; i++)
         newProg[vmem_offset + i] = prog[file_offset + i];

      //int rw = (prog_header->flags & 0x1) == 0x1;

      //gui_window_writestr("\nVirtual addr: ", 0, 0);
      //gui_window_writeuint(prog_header->p_vaddr, 0, 0);

      //gui_window_writestr("\nAlignment: ", 0, 0);
      //gui_window_writeuint(prog_header->alignment, 0, 0);

      /*gui_window_writestr("\nMapping ", 0, 0);
      gui_window_writeuint((uint32_t)newProg + vmem_offset, 0, 0);
      gui_window_writestr(" to ", 0, 0);
      gui_window_writeuint(prog_header->p_vaddr, 0, 0);
      gui_window_writestr(" with size ", 0, 0);
      gui_window_writeuint((uint32_t)prog_header->p_memsz, 0, 0);
      gui_window_writestr("\n", 0, 0);*/
      
      prog_header++;
   }

   uint32_t offset = elf_header->entry - vmem_start;

   gui_writestr("\nOffset: ", 0);
   gui_writeuint(offset, 0);
   gui_writestr("\n", 0);

   //gui_draw();
   //while(true);

   create_task_entry(index, elf_header->entry, (vmem_end - vmem_start), false);
   gettasks()[index].vmem_start = vmem_start;
   gettasks()[index].vmem_end = vmem_end;
   gettasks()[index].prog_start = (uint32_t)newProg;
   launch_task(index, regs, true);

   // push args
   regs->useresp -= 4;
   ((uint32_t*)regs->useresp)[0] = (uint32_t)args; // char **args
   regs->useresp -= 4;
   ((uint32_t*)regs->useresp)[0] = argc; // int argc
   regs->useresp -= 4;
   ((uint32_t*)regs->useresp)[0] = 0; // push dummy return addr

   gui_writestr("\nStarting at: ", 0);
   gui_writeuint(elf_header->entry, 0);
   gui_writestr("\n", 0);

}