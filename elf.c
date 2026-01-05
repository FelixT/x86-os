// elf32
// https://refspecs.linuxbase.org/elf/gabi4+/ch5.pheader.html

#include "elf.h"
#include "paging.h"
#include "windowmgr.h"
#include "memory.h"
#include "tasks.h"
#include "memory.h"

void elf_run(registers_t *regs, uint8_t *prog, uint32_t size, int argc, char **args, bool focus) {

   int task_index = get_free_task_index();
   if(task_index == -1) {
      debug_printf("No free tasks\n");
      return;
   }

   elf_header_t *elf_header = (elf_header_t*)prog;
   elf_prog_header_t *prog_header = (elf_prog_header_t*)(prog + elf_header->prog_header);
   // fail if
   // elf_header->type != 2 i.e. not exec
   // elf_header->prog_header_entry_count == 0
   // elf signature invalid
   if(elf_header->type != 2) {
      debug_printf("\nELF Type %u is not supported\n", elf_header->type);
      return;
   }
   if(memcmp(elf_header->magic, "\x7F""ELF", 4) != 0) {
      debug_printf("Invalid ELF signature\n");
      return;
   }
   // Work out how much memory to assign

   // get virtual memory start addr, end address
   uint32_t vmem_start = 0xFFFFFFFF;
   uint32_t vmem_end = 0;
   
   elf_prog_header_t *headers = (elf_prog_header_t*)(prog + elf_header->prog_header);
   
   for(int i = 0; i < elf_header->prog_header_entry_count; i++) {
      elf_prog_header_t *ph = &headers[i];
   
      if(ph->segment_type != 1) // Only LOAD segments
         continue;

      if(ph->p_offset + ph->p_filesz > size) {
         debug_printf("ELF segment %u exceeds binary size \n", i, ph->p_offset, ph->p_filesz, size);
         return;
      }
   
      if(ph->p_vaddr < vmem_start)
         vmem_start = ph->p_vaddr;
   
       if(ph->p_vaddr + ph->p_memsz > vmem_end)
         vmem_end = ph->p_vaddr + ph->p_memsz;
   }
   
   if(vmem_start == 0xFFFFFFFF || vmem_end == 0) {
      debug_printf("No valid LOAD segments in ELF\n");
      return;
   }
   
   uint32_t vmem_size = vmem_end - vmem_start;
   
   // create page directory
   page_dir_entry_t *dir = new_page();

   if(vmem_start <= KERNEL_END && vmem_end >= KERNEL_START) {
      // if virtual memory addresses conflict with kernel space
      debug_printf("ELF virtual addresses conflict with kernel space\n");
      return;
   }

   // allocate memory
   uint8_t *newProg = malloc(vmem_size);
   // fill with 0s
   memset(newProg, 0, vmem_size);

   // start of newProg maps to vmem_start  

   prog_header = (elf_prog_header_t*)(prog + elf_header->prog_header);

   debug_printf("Mapping 0x%h - 0x%h to 0x%h - 0x%h\n", (uint32_t)newProg, (uint32_t)newProg + vmem_size, vmem_start, vmem_end);

   // map vmem
   for(uint32_t i = 0; i < vmem_size; i+=0x1000)
      map(dir, (uint32_t)newProg + i, vmem_start + i, 1, 1);

   uint32_t heap_start = page_align_up(vmem_end);
   debug_printf("Heap start 0x%h\n", heap_start);

   debug_printf("Start 0x%h\n", page_getphysical(dir, elf_header->entry));

   // copy program to new location and assign virtual memory for each segment
   for(int i = 0; i < elf_header->prog_header_entry_count; i++) {
      if(prog_header->segment_type != 1) {
         // if not LOAD
         prog_header++;
         continue;
      }

      uint32_t file_offset = prog_header->p_offset;
      uint32_t vmem_offset = prog_header->p_vaddr - vmem_start;

      debug_printf("Header %i vaddr 0x%h size 0x%h\n", i, prog_header->p_vaddr, prog_header->p_memsz);

      // copy
      memcpy_fast(&newProg[vmem_offset], &prog[file_offset], (int)prog_header->p_filesz);

      if(prog_header->p_memsz > prog_header->p_filesz)
         memset(&newProg[vmem_offset] + prog_header->p_filesz, 0, prog_header->p_memsz - prog_header->p_filesz);

      //int rw = (prog_header->flags & 0x1) == 0x1;
      
      prog_header++;
   }

   // create new process
   create_task_entry(task_index, elf_header->entry, (vmem_end - vmem_start), false, NULL);
   task_state_t *task = &gettasks()[task_index];
   task->process->vmem_start = vmem_start;
   task->process->vmem_end = vmem_end;
   task->process->prog_start = (uint32_t)newProg;
   task->process->page_dir = dir;
   task->process->heap_start = heap_start;
   task->process->heap_end = heap_start;
   launch_task(task_index, regs, focus);

   // push args
   regs->useresp -= 4;
   ((uint32_t*)regs->useresp)[0] = (uint32_t)args; // char **args
   regs->useresp -= 4;
   ((uint32_t*)regs->useresp)[0] = argc; // int argc
   regs->useresp -= 4;
   ((uint32_t*)regs->useresp)[0] = vmem_start; // push dummy (mapped) return addr

}