#include <stdint.h>

#include "prog.h"
#include "prog_wo.h"
#include "../lib/string.h"
#include "lib/wo_api.h"
#include "lib/stdio.h"

typedef struct {
   uint32_t headerSize; // size of this header
   int32_t width;
   int32_t height;
   uint16_t colourPlanes;
   uint16_t bpp;
   uint32_t compressionMethod;
   uint32_t dataSize;
   uint32_t horizontalRes;
   uint32_t verticalRes;
   uint32_t colourPaletteLength;
   uint32_t importantColours;
} __attribute__((packed)) bmp_info_t;

typedef struct {
   uint16_t identifier; // 'BM'
   uint32_t size; // bytes
   uint32_t reserved;
   uint32_t dataOffset; // pixel data
} __attribute__((packed)) bmp_header_t;

uint16_t *framebuffer;
uint8_t *bmp;
uint16_t *bmpbuffer;
uint32_t rowSize;
bmp_info_t *info;
int bufferwidth = 0;
int width = 0;
int height = 0;
char path[256];

windowobj_t *wo_menu;
windowobj_t *options = NULL;

windowobj_t *colour_wo;
windowobj_t *zoominbtn_wo;
windowobj_t *zoomtext_wo;
windowobj_t *zoomoutbtn_wo;
windowobj_t *openbtn_wo;
windowobj_t *savebtn_wo;
windowobj_t *toolminusbtn_wo;
windowobj_t *toolsizetext_wo;
windowobj_t *toolplusbtn_wo;
int size = 2;
int scale = 1;
int colour = 0;
int offsetX = 0;
int offsetY = 0;
FILE *current_file = NULL;

void resize(uint32_t fb, uint32_t w, uint32_t h) {
   framebuffer = (uint16_t*)fb;

   width = w;
   height = h;
   bufferwidth = get_surface().width;
   wo_menu->y = height - wo_menu->height;
   wo_menu->width = w;
   clear();
   bmp_draw((uint8_t*)bmp, -offsetX, -offsetY, scale, false);
   redraw();
   end_subroutine();
}

static inline int min(int a, int b) { return (a < b) ? a : b; }
static inline int max(int a, int b) { return (a > b) ? a : b; }

int brush_type = 0;

static const uint8_t brush_patterns[][8] = {
   { // default
      0b01111110,
      0b11111111,
      0b11111111,
      0b11111111,
      0b11111111,
      0b11111111,
      0b11111111,
      0b01111110,
   },
   { // circle
      0b00011000,
      0b01111110,
      0b11111111,
      0b11111111,
      0b11111111,
      0b11111111,
      0b01111110,
      0b00011000,
   },
   { // square
      0b11111111,
      0b11111111,
      0b11111111,
      0b11111111,
      0b11111111,
      0b11111111,
      0b11111111,
      0b11111111,
   }, // cross
   {
      0b00011000,
      0b00011000,
      0b00011000,
      0b11111111,
      0b11111111,
      0b00011000,
      0b00011000,
      0b00011000,
   }, // spray
   {
      0b01000010,
      0b00100100,
      0b10010001,
      0b01001010,
      0b00100100,
      0b10010001,
      0b01000010,
      0b00100100,
   }, // custom (todo)
   {
      0b11111111,
      0b11111111,
      0b11111111,
      0b11111111,
      0b11111111,
      0b11111111,
      0b11111111,
      0b11111111,
   },
};

void set(int x, int y) {
   int brush_size = size * scale;
   
   if(x < 0 || x >= info->width || x >= bufferwidth || y < 0 || y >= height || y >= info->width) return;

   int start_x = max(0, x);
   int end_x = min(bufferwidth, x + brush_size);
   int start_y = max(0, y);
   int end_y = min(height, y + brush_size);
   
   for(int py = start_y; py < end_y; py++) {
      for(int px = start_x; px < end_x; px++) {
         int pattern_y = ((py-y) * 8) / brush_size;
         int pattern_x = ((px-x) * 8) / brush_size;
         
         // check pattern
         if(size > 1 && !(brush_patterns[brush_type][pattern_y] & (1 << (7 - pattern_x)))) 
            continue;

         framebuffer[px + py * bufferwidth] = colour;

         // set bmp
         if(info->bpp != 16) return;
         int rowOffset = (info->height - ((py + offsetY)/scale + 1)) * rowSize;
         int index = rowOffset / 2 + px/scale;
         if(index >= (int)info->dataSize) return;
         bmpbuffer[index] = colour;
      }
   }
   
   //redraw_pixel(x, y);
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

   if(x > width) // clicked scrollarea
      end_subroutine();

   if(options && options->visible)
      end_subroutine();

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

void tool_click(void *wo, void *regs) {
   (void)wo;
   (void)regs;
   size++;
   uinttostr(size, toolsizetext_wo->text);

   end_subroutine();

}

void toolminus_click(void *wo, void *regs) {
   (void)wo;
   (void)regs;
   size--;
   if(size < 1) size = 1;
   uinttostr(size, toolsizetext_wo->text);

   end_subroutine();

}

void zoomout_click(void *wo, void *regs) {
   (void)wo;
   (void)regs;
   if(scale > 1) scale--;
   sprintf(zoomtext_wo->text, "%i00%", scale);
   clear();
   bmp_draw((uint8_t*)bmp, 0, 0, scale, false);
   set_content_height(info->height*scale + 20);
   end_subroutine();
}

void zoomin_click(void *wo, void *regs) {
   (void)wo;
   (void)regs;
   scale++;
   sprintf(zoomtext_wo->text, "%i00%", scale);
   clear();
   bmp_draw((uint8_t*)bmp, 0, 0, scale, false);
   set_content_height(info->height*scale + 20);
   end_subroutine();
}

void colour_callback(uint16_t c) {
   colour = c;
   colour_wo->colour_bg = c;
   colour_wo->colour_bg_hover = c;
   end_subroutine();
}

void colour_click(void *wo, void *regs) {
   (void)wo;
   (void)regs;
   display_colourpicker(colour, &colour_callback);
   end_subroutine();
}

bool bmp_check() {
   // check if supported
   if(bmp[0] != 'B' || bmp[1] != 'M') {
      display_popup("Error", "File isn't a BMP\n", false, NULL);
      return false;
   }
   if(info->bpp != 8 && info->bpp != 16) {
      display_popup("Error", "BPP not 8bit or 16bit\n", false, NULL);
      return false;
   }
   if(info->bpp == 8 && info->compressionMethod != 0) {
      display_popup("Error", "Compression method not BI_RGB for 8bit bmp\n", false, NULL);
      return false;
   }
   if(info->bpp == 16 && info->compressionMethod != 3) {
      display_popup("Error", "Compression method not BI_BITFIELDS for 16bit bmp\n", false, NULL);
      return false;
   }
   return true;
}

void load_img() {
   // close existing
   if(current_file)
      fclose(current_file);

   FILE *f = fopen(path, "r+");
   if(!f) {
      display_popup("Error", "File not found\n", false, NULL);
      exit(0);
   }
   int size = fsize(fileno(f));
   char *buf = (char*)malloc(size);
   if(!fread(buf, size, 1, f)) {
      display_popup("Error", "Couldn't read file\n", false, NULL);
      exit(0);
   }
   current_file = f;

   bmp = (uint8_t*)buf;

   info = (bmp_info_t*)(&bmp[sizeof(bmp_header_t)]);
   if(bmp_check())
      bmp_draw((uint8_t*)bmp, 0, 0, scale, false);

   bmp_header_t *header = (bmp_header_t*)bmp;
   bmpbuffer = (uint16_t*)(&bmp[0] + header->dataOffset);
   rowSize = ((info->width * 2 + 3) / 4) * 4;
}

// filepicker callback
void open_file(char *p) {
   clear();
   strcpy(path, p);

   load_img();

   set_content_height(info->height*scale + 20);
   
   end_subroutine();
}

void open_click(void *wo, void *regs) {
   (void)wo;
   (void)regs;
   display_filepicker(&open_file);
   end_subroutine();
}

void save_click(void *wo, void *regs) {
   (void)wo;
   (void)regs;
   if(info->bpp != 16) {
      display_popup("Error", "Can only save 16bit bitmaps\n", false, NULL);
      end_subroutine();
      return;
   }

   bmp_header_t *header = (bmp_header_t*)bmp;
   
   height -= wo_menu->height; // remove menu bar

   int maxX = info->width; // bitmap width (not scaled)
   if(width/scale < maxX)
      maxX = width/scale;
   int maxY = info->height; // bitmap height (not scaled)  
   if(height/scale < maxY)
      maxY = height/scale;

   uint32_t rowSize = ((info->width * 2 + 3) / 4) * 4;

   uint32_t size = header->dataOffset + rowSize * info->height;
   fseek(current_file, 0, SEEK_SET);
   fwrite(bmp, size, 1, current_file);
   fflush(current_file);

   height += wo_menu->height; // restore

   end_subroutine();
}

void scroll(int deltaY, int offY) {
   (void)deltaY;
   offsetY = offY;
   bmp_draw((uint8_t*)bmp, -offsetX, -offsetY, scale, false);
   end_subroutine();
}

void brush_callback(windowobj_t *wo) {
   brush_type = wo->menuselected;

   end_subroutine();
}

void brush_close() {
   options->visible = false;
   clear();
   bmp_draw((uint8_t*)bmp, -offsetX, -offsetY, scale, false);

   end_subroutine();
}

void brush_click(void *wo) {
   (void)wo;

   if(options != NULL) {
      options->visible = true;
      end_subroutine();
   }

   // create canvas
   options = create_canvas(NULL, 20, 20, 100, 120);
   create_text_static(options, 5, 8, "Brush type:");
   windowobj_t *menu = create_wo(options, WO_MENU, 5, 20, 90, 75);
   windowobj_t *close = create_button(options, 5, 100, "Close");
   close->release_func = &brush_close;
   menu->menuitems = (windowobj_menu_t*)malloc(sizeof(windowobj_menu_t) * 2);
   windowobj_menu_t generic;
   generic.disabled = false;
   generic.func = &brush_callback;

   menu->menuitem_count = 5;
   for(int i = 0; i < menu->menuitem_count; i++)
      menu->menuitems[i] = generic;
   strcpy(menu->menuitems[0].text, "Default");
   strcpy(menu->menuitems[1].text, "Circle");
   strcpy(menu->menuitems[2].text, "Square");
   strcpy(menu->menuitems[3].text, "Cross");
   strcpy(menu->menuitems[4].text, "Spray");
   end_subroutine();
}

// discard changes
void clear_click(void *wo, void *regs) {
   (void)wo;
   (void)regs;
   load_img(path);
   end_subroutine();
}

void _start(int argc, char **args) {
   set_window_title("BMP Viewer");

   if(argc != 1) {
      printf("Wrong number of arguments provided\n");
      exit(0);
   }

   if(*args[0] != '\0') {
      if(args[0][0] != '/') {
         // relative path
         getwd(path);
         if(!strequ(path, "/"))
            strcat(path, "/");
         strcat(path, args[0]);
      } else {
         // absolute path
         strcpy(path, args[0]);
      }
   }
   
   load_img();

   override_click((uint32_t)&click);
   override_drag((uint32_t)&click);
   override_mouserelease((uint32_t)&release);
   override_draw((uint32_t)NULL);
   override_resize((uint32_t)&resize);
   clear();

   framebuffer = (uint16_t*)(get_surface().buffer);
   bufferwidth = get_surface().width;
   width = get_width();
   height = get_height();

   // menu
   wo_menu = create_canvas(NULL, 0, height - 18, width, 18);
   wo_menu->bordered = false;
   // window objects
   int margin = 3;
   int x = margin;
   int y = 2;

   zoomoutbtn_wo = create_button(wo_menu, x, y,  "-");
   zoomoutbtn_wo->width = 15;
   zoomoutbtn_wo->click_func = &zoomout_click;
   x += zoomoutbtn_wo->width;

   zoomtext_wo = create_text(wo_menu, x, y, "100%");
   zoomtext_wo->width = 40;
   zoomtext_wo->texthalign = true;
   zoomtext_wo->textvalign = true;
   x += zoomtext_wo->width;

   zoominbtn_wo = create_button(wo_menu, x, y, "+");
   zoominbtn_wo->width = 15;
   zoominbtn_wo->click_func = &zoomin_click;
   x += zoominbtn_wo->width + margin;

   openbtn_wo = create_button(wo_menu, x, y, "Open");
   openbtn_wo->width = 40;
   openbtn_wo->click_func = &open_click;
   x += openbtn_wo->width + margin;

   savebtn_wo = create_button(wo_menu, x, y, "Save");
   savebtn_wo->width = 40;
   savebtn_wo->click_func = &save_click;
   x += savebtn_wo->width + margin;

   windowobj_t *clearbtn_wo = create_button(wo_menu, x, y, "Reset");
   clearbtn_wo->width = 40;
   clearbtn_wo->click_func = &clear_click;
   x += clearbtn_wo->width + margin;
   
   windowobj_t *brushtxt_wo = create_text_static(wo_menu, x, y, "Brush: ");
   brushtxt_wo->width = 40;
   brushtxt_wo->textvalign = true;
   brushtxt_wo->click_func = (void*)&brush_click;
   x += brushtxt_wo->width;

   colour_wo = create_canvas(wo_menu, x, y, 14, 14);
   colour_wo->colour_bg = 0;
   colour_wo->colour_bg_hover = 0;
   colour_wo->click_func = &colour_click;
   x += colour_wo->width + margin;

   toolminusbtn_wo = create_button(wo_menu, x, y, "-");
   toolminusbtn_wo->width = 15;
   toolminusbtn_wo->click_func = &toolminus_click;
   x += toolminusbtn_wo->width;

   toolsizetext_wo = create_text(wo_menu, x, y, "2");
   toolsizetext_wo->width = 20;
   toolsizetext_wo->texthalign = true;
   toolsizetext_wo->textvalign = true;
   toolsizetext_wo->textpadding = 0;
   x += toolsizetext_wo->width;

   toolplusbtn_wo = create_button(wo_menu, x, y, "+");
   toolplusbtn_wo->width = 15;
   toolplusbtn_wo->click_func = &tool_click;
   x += toolplusbtn_wo->width + margin;

   redraw();

   create_scrollbar(&scroll);
   set_content_height(info->height*scale + 20);

   while(1==1) {
      //asm volatile("pause");
      yield();
   }

   exit(0);

}