#ifndef ELF_H
#define ELF_H

#include <stdint.h>
#include <stdbool.h>
#include "registers_t.h"

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

void elf_run(registers_t *regs, uint8_t *prog, uint32_t size, int argc, char **args, bool focus);

#endif