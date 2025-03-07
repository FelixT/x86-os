#include <stdint.h>

#include "prog.h"

typedef struct {
   uint8_t filename[11];
   uint8_t attributes;
   uint8_t reserved;
   uint8_t creationTimeFine; // in 10ths of a second
   uint16_t creationTime;
   uint16_t creationDate;
   uint16_t lastAccessDate;
   uint16_t zero; // high 16 bits of entry's first cluster no in other fat vers. we can use this for position
   uint16_t lastModifyTime;
   uint16_t lastModifyDate;
   uint16_t firstClusterNo;
   uint32_t fileSize; // bytes
} __attribute__((packed)) fat_dir_t;

uint16_t *framebuffer;
uint8_t *bmp;
int width = 0;

void strcpy(char* dest, char* src) {
   int i = 0;

   while(src[i] != '\0') {
      dest[i] = src[i];
      i++;
   }
   dest[i] = '\0';
}

void click(int x, int y) {
   framebuffer[x+y*width] = 0;
   framebuffer[x+1+y*width] = 0;
   framebuffer[x+(y+1)*width] = 0;
   framebuffer[x+1+(y+1)*width] = 0;
   redraw();

   end_subroutine();

}

void clear_click(void *wo) {
   (void)wo;
   clear(0xFFFF);

   bmp_draw((uint8_t*)bmp, 0, 0);

   end_subroutine();

}

void _start(int argc, char **args) {

   if(argc != 1) {
      write_str("Wrong number of arguments provided");
      exit(0);
   }

   char *path = (char*)args[0];

   write_str(path);
   write_str("\n");
   fat_dir_t *entry = (fat_dir_t*)fat_parse_path(path);
   if(entry == NULL) {
      write_str("File not found\n");
      exit(0);
   }

   override_click((uint32_t)&click);
   override_draw((uint32_t)NULL);
   clear(0xFFFF);
   
   bmp = (uint8_t*)fat_read_file(entry->firstClusterNo, entry->fileSize);
   bmp_draw((uint8_t*)bmp, 0, 0);

   // warning: this will change if the window is resized
   // ideally this should be mapped to a constant vmem location
   framebuffer = (uint16_t*)get_framebuffer();
   width = get_width();

   // window objects
   windowobj_t *wo_clear = register_windowobj();
   wo_clear->type = WO_BUTTON;
   wo_clear->text = (char*)malloc(1);
   wo_clear->x = wo_clear->window_surface->width - wo_clear->width - 2;
   strcpy(wo_clear->text, "CLEAR");
   wo_clear->click_func = &clear_click;

   /*windowobj_t *wo_save = register_windowobj();
   wo_save->type = WO_BUTTON;
   wo_save->text = (char*)malloc(1);
   wo_save->x = wo_save->window_surface->width - wo_save->width - 2;
   wo_save->y = 40;
   strcpy(wo_save->text, "SAVE");
   wo_save->click_func = &clear_click;*/

   redraw();

   while(1==1) {
      asm volatile("nop");
   }

   exit(0);

}