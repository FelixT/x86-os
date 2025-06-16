#include <stdint.h>

#include "prog.h"
#include "../lib/string.h"

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
int height = 0;

windowobj_t *clearbtn_wo;
windowobj_t *toolbtn_wo;
windowobj_t *zoominbtn_wo;
windowobj_t *zoomoutbtn_wo;
int size = 1; 
int scale = 1;
int colour = 0;

void resize(uint32_t fb, uint32_t w, uint32_t h) {
   framebuffer = (uint16_t*)fb;

   width = w;
   height = h;
   clearbtn_wo->x = width - (clearbtn_wo->width + 10);
   toolbtn_wo->x = width - (toolbtn_wo->width + 65);
   clearbtn_wo->y = height - (clearbtn_wo->height + 10);
   toolbtn_wo->y = height - (toolbtn_wo->height + 10);
   zoomoutbtn_wo->x = width - (clearbtn_wo->width + 130);
   zoomoutbtn_wo->y = height - (clearbtn_wo->height + 10);
   zoominbtn_wo->x = width - (clearbtn_wo->width + 110);
   zoominbtn_wo->y = height - (clearbtn_wo->height + 10);
   clear();
   bmp_draw((uint8_t*)bmp, 0, 0, scale);
   redraw();
   end_subroutine();
}

void set(int x, int y) {
   framebuffer[x+y*width] = colour;
   redraw_pixel(x, y);
   for(int i = 1; i < size; i++) {
      framebuffer[x+i+y*width] = colour;
      framebuffer[x+(y+i)*width] = colour;
      framebuffer[x+i+(y+i)*width] = colour;
   }
}

int abs(int i) {
   if(i < 0) return -i;
   return i;
}

void drawLine(int x0, int y0, int x1, int y1) {
   // bresenham's line algorithm
   int dx = abs(x1 - x0);
   int sx = x0 < x1 ? 1 : -1;
   int dy = -abs(y1 - y0);
   int sy = y0 < y1 ? 1 : -1;
   int error = dx + dy;
   int e2;
   
   while(1) {
      set(x0, y0);  // set the current pixel
       
      // check if we've reached the end point
      if(x0 == x1 && y0 == y1) 
         break;
           
      e2 = 2 * error;
       
      // update x if needed
      if(e2 >= dy) {
         if(x0 == x1) 
            break;
         error += dy;
         x0 += sx;
       }
       
      // update y if needed
      if(e2 <= dx) {
         if(y0 == y1) 
            break;
         error += dx;
         y0 += sy;
      }
   }
}

int prevX = -1;
int prevY = -1;

void click(int x, int y) {

   set(x, y);
   if(prevX == -1) prevX = x;
   if(prevY == -1) prevY = y;

   int deltaX = abs(x - prevX);
   int deltaY = abs(y - prevY);

   if(deltaX >= 1 || deltaY >= 1)
      drawLine(prevX, prevY, x, y);

   prevX = x;
   prevY = y;

   end_subroutine();

}

void release() {
   prevX = -1;
   prevY = -1;

   end_subroutine();
}

void clear_click(void *wo, void *regs) {
   (void)wo;
   (void)regs;
   clear();

   bmp_draw((uint8_t*)bmp, 0, 0, scale);

   end_subroutine();

}

void tool_click(void *wo, void *regs) {
   (void)wo;
   (void)regs;
   size++;

   end_subroutine();

}

void zoomout_click(void *wo, void *regs) {
   (void)wo;
   (void)regs;
   if(scale > 1) scale--;
   clear();
   bmp_draw((uint8_t*)bmp, 0, 0, scale);
   end_subroutine();
}

void zoomin_click(void *wo, void *regs) {
   (void)wo;
   (void)regs;
   scale++;
   clear();
   bmp_draw((uint8_t*)bmp, 0, 0, scale);
   end_subroutine();
}

void colour_callback(uint16_t c) {
   colour = c;
   end_subroutine();
}

void colour_click(void *wo, void *regs) {
   (void)wo;
   (void)regs;
   display_colourpicker(&colour_callback);
   end_subroutine();
}

void _start(int argc, char **args) {
   set_window_title("BMP Viewer");

   if(argc != 1) {
      write_str("Wrong number of arguments provided");
      exit(0);
   }

   char path[256];

   if(*args[0] != '\0') {
      if(args[0][0] != '/') {
         // relative path
         getwd(path);
         if(!strcmp(path, "/"))
            strcat(path, "/");
         strcat(path, args[0]);
      } else {
         // absolute path
         strcpy(path, args[0]);
      }
   }

   write_str(path);
   write_str("\n");
   fat_dir_t *entry = (fat_dir_t*)fat_parse_path(path, true);
   if(entry == NULL) {
      write_str("File not found\n");
      exit(0);
   }

   override_click((uint32_t)&click);
   override_drag((uint32_t)&click);
   override_mouserelease((uint32_t)&release);
   override_draw((uint32_t)NULL);
   override_resize((uint32_t)&resize);
   clear();
   
   bmp = (uint8_t*)fat_read_file(entry->firstClusterNo, entry->fileSize);
   bmp_draw((uint8_t*)bmp, 0, 0, scale);

   framebuffer = (uint16_t*)get_framebuffer();
   width = get_width();
   height = get_height();

   // window objects
   int margin = 5;
   int x = margin;
   int y = height - 30;
   windowobj_t *clearbtn = register_windowobj(WO_BUTTON, x, y, 50, 14);
   clearbtn->text = (char*)malloc(1);
   strcpy(clearbtn->text, "Clear");
   clearbtn->click_func = &clear_click;
   clearbtn_wo = clearbtn;

   x += clearbtn->width + margin;

   windowobj_t *toolbtn = register_windowobj(WO_BUTTON, x, y, 50, 14);
   toolbtn->text = (char*)malloc(1);
   strcpy(toolbtn->text, "Size");
   toolbtn->click_func = &tool_click;
   toolbtn_wo = toolbtn;

   x += toolbtn->width + margin;

   windowobj_t *colourbtn = register_windowobj(WO_BUTTON, x, y, 50, 14);
   colourbtn->text = (char*)malloc(1);
   strcpy(colourbtn->text, "Colour");
   colourbtn->click_func = &colour_click;
   colourbtn = toolbtn;

   x += colourbtn->width + margin;

   windowobj_t *zoominbtn = register_windowobj(WO_BUTTON, x, y, 20, 14);
   zoominbtn->text = (char*)malloc(1);
   strcpy(zoominbtn->text, "+");
   zoominbtn->click_func = &zoomin_click;
   zoominbtn_wo = zoominbtn;

   x += zoominbtn_wo->width + margin;

   windowobj_t *zoomoutbtn = register_windowobj(WO_BUTTON, x, y, 20, 14);
   zoomoutbtn->text = (char*)malloc(1);
   strcpy(zoomoutbtn->text, "-");
   zoomoutbtn->click_func = &zoomout_click;
   zoomoutbtn_wo = zoomoutbtn;
   redraw();

   while(1==1) {
      //asm volatile("pause");
      yield();
   }

   exit(0);

}