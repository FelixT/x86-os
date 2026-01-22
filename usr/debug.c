// debugger

#include "lib/dialogs.h"
#include "prog.h"
#include "lib/stdio.h"
#include "../lib/string.h"
#include "lib/sort.h"

// from elf.h
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

// https://refspecs.linuxbase.org/elf/gabi4+/ch4.sheader.html
typedef struct {
   uint32_t sh_name;
   uint32_t sh_type;
   uint32_t sh_flags;
   uint32_t sh_addr;
   uint32_t sh_offset;
   uint32_t sh_size;
   uint32_t sh_link;
   uint32_t sh_info;
   uint32_t sh_addralign;
   uint32_t sh_entsize;
} __attribute__((packed)) Elf32_Shdr;

// https://refspecs.linuxbase.org/elf/gabi4+/ch4.symtab.html
typedef struct {
	uint32_t	st_name;
	uint32_t	st_value;
	uint32_t	st_size;
	uint8_t st_info;
	uint8_t st_other;
	uint16_t	st_shndx;
} Elf32_Sym;

#define SHT_NULL 0
#define SHT_SYMTAB 2
#define SHT_STRTAB 3
#define SHT_DYNSYM 11

#define STT_FUNC 2
#define STT_FILE 4

#define ELF32_ST_BIND(i) ((i)>>4)
#define ELF32_ST_TYPE(i) ((i)&0xf)

Elf32_Sym *symbols = NULL;
char *strings = NULL;
int num_symbols = 0;

dialog_t *dialog;
wo_t *menu;

int sort_func(void *a, void *b) {
   Elf32_Sym *sa = a;
   Elf32_Sym *sb = b;
   return sa->st_value - sb->st_value;
}

void show_symbols(elf_header_t *elf) {
   // parse elf file to find symbol & string tables
   Elf32_Shdr *sections = (Elf32_Shdr*)((char*)elf + elf->section_header);
        
   Elf32_Shdr *symtab = NULL;
   Elf32_Shdr *strtab = NULL;
   
   for(int i = 0; i < elf->section_header_entry_count; i++) {
      Elf32_Shdr *sh = &sections[i];
      
      if(sh->sh_type == SHT_SYMTAB || sh->sh_type == SHT_DYNSYM) {
         symtab = sh;
         strtab = &sections[sh->sh_link];
         break;
      }
   }
   
   if(!symtab || !strtab) {
      debug_println("No symbol table found");
      return;
   }
   
   symbols = (Elf32_Sym*)((char*)elf + symtab->sh_offset);
   strings = (char*)elf + strtab->sh_offset;
   num_symbols = symtab->sh_size / sizeof(Elf32_Sym);
   
   // show in menu

   // sort
   // sort(symbols, num_symbols, sizeof(Elf32_Sym), &sort_func);

   for(int i = 0; i < num_symbols; i++) {
      Elf32_Sym *sym = &symbols[i];
      if(sym->st_name == 0) continue;
      if(ELF32_ST_TYPE(sym->st_info) == STT_FILE) {
         char *name = strings + sym->st_name;
         char buf[64];
         snprintf(buf, 63, "file %s", name);
         add_menu_item(menu, buf, NULL);
      }
      if(ELF32_ST_TYPE(sym->st_info) == STT_FUNC) {
         char *name = strings + sym->st_name;
         char buf[64];
         snprintf(buf, 63, " 0x%h: %s (size: %u)", sym->st_value, name, sym->st_size);
         add_menu_item(menu, buf, NULL);
      }
   }
}

void validate_elf(elf_header_t *elf) {
   if(memcmp(elf->magic, "\x7F""ELF", 4) != 0) {
      dialog_msg("Error", "Invalid ELF signature");
      return;
   }

   elf_prog_header_t *headers = (elf_prog_header_t*)(elf + elf->prog_header);
   
   uint32_t vmem_start = 0xFFFFFFFF;
   uint32_t vmem_end = 0;
   for(int i = 0; i < elf->prog_header_entry_count; i++) {
      elf_prog_header_t *ph = &headers[i];

      if(ph->p_vaddr < vmem_start)
         vmem_start = ph->p_vaddr;
   
      if(ph->p_vaddr + ph->p_memsz > vmem_end)
         vmem_end = ph->p_vaddr + ph->p_memsz;
   }
   show_symbols(elf);
}

void load_elf(char *path) {
   // read header
   FILE *f = fopen(path, "r");
   int size = fsize(fileno(f));
   elf_header_t *elf = malloc(size);
   fread(elf, size, 1, f);
   fclose(f);
   validate_elf(elf);
   //free(elf, size);
}

void browse_callback(char *path, int window, wo_t *wo) {
   (void)window;
   (void)wo;
   menu_t *menu_data = menu->data;
   free(menu_data->items, sizeof(menu_item_t)*menu_data->item_count);
   menu_data->items = NULL;
   menu_data->item_count = 0;
   menu_data->selected_index = -1;
   menu_data->offset = 0;
   load_elf(path);
   ui_draw(dialog->ui);
}

void search_callback(wo_t *input, int window) {
   (void)window;
   char *txt = get_input(input)->text;
   uint32_t addr = hextouint(txt);

   int found = -1;
   char *curfilename;
   int menu_i = 0;
   for(int i = 0; i < num_symbols; i++) {
      Elf32_Sym *sym = &symbols[i];
      if(sym->st_name == 0) continue;
      if(ELF32_ST_TYPE(sym->st_info) == STT_FILE) {
         curfilename = strings + sym->st_name;
         menu_i++;
      }
      if(ELF32_ST_TYPE(sym->st_info) == STT_FUNC && sym->st_name != 0) {
         if(addr >= sym->st_value && addr < sym->st_value + sym->st_size) {
            char buf[128];
            char *name = strings + sym->st_name;
            snprintf(buf, 127, "Found function %s\nat 0x%h in %s", name, sym->st_value, curfilename);
            dialog_msg("Msg", buf);
            found = menu_i;
         }
         menu_i++;
      }
   }

   if(found == -1) {
      dialog_msg("Msg", "Couldn't find location");
   } else {
      menu_t* menu_data = menu->data;
      menu_data->selected_index = found;
      menu_data->offset = menu_data->selected_index - menu_data->shown_items + 1;
      if(menu_data->offset < 0)
         menu_data->offset = 0;
      ui_draw(dialog->ui);
   }
}

void _start(int argc, char **args) {
   int index = get_free_dialog();
   dialog = get_dialog(index);
   dialog_init(dialog, -1);
   dialog_set_title(dialog, "CrashManager");

   menu = create_menu(5, 5, 345, 215);
   ui_add(dialog->ui, menu);

   wo_t *browsebtn = dialog_create_browsebtn(5, 225, 45, 20, -1, "Browse", "/sys", &browse_callback);
   ui_add(dialog->ui, browsebtn);

   wo_t *searchinput = create_input(60, 225, 60, 20);
   set_input_return(searchinput, &search_callback);
   ui_add(dialog->ui, searchinput);

   debug_println("Argc %i", argc);

   char *file = "/sys/debug.elf";
   if(argc > 1)
      file = args[1];
   debug_println("Loading file %s", file);
   load_elf(file);

   ui_draw(dialog->ui);

   if(argc > 2) {
      char *addrhex = args[2];
      uint32_t addr = hextouint(addrhex);
      debug_println("Looking for address 0x%h", addr);
      set_input_text(searchinput, addrhex);
      search_callback(searchinput, -1);
   }

   ui_draw(dialog->ui);

   while(true) {
      yield();
   }

}