#include <stdint.h>

#include "prog.h"
#include "../lib/string.h"
#include "lib/stdio.h"
#include "lib/dialogs.h"
#include "prog_bmp.h"
#include "lib/draw.h"

uint16_t *framebuffer;
uint8_t *bmp;
uint16_t *bmpbuffer;
uint32_t rowSize;
bmp_info_t *info;
int bufferwidth = 0;
int width = 0;
int height = 0;
char path[256];
surface_t surface;
ui_mgr_t *ui;

wo_t *wo_menu; // canvas
wo_t *options = NULL;

wo_t *zoominbtn_wo;
wo_t *zoomtext_wo;
wo_t *zoomoutbtn_wo;
wo_t *openbtn_wo;
wo_t *savebtn_wo;
wo_t *toolminusbtn_wo;
wo_t *toolsizetext_wo;
wo_t *toolplusbtn_wo;
int size = 2;
int scale = 1;
int colour = 0;
int offsetX = 0;
int offsetY = 0;
FILE *current_file = NULL;

void drawbg() {
   draw_checkeredrect(&surface, 0xBDF7, 0xDEDB, 0, 0, width, height);
}

void resize(uint32_t fb, uint32_t w, uint32_t h) {
   framebuffer = (uint16_t*)fb;

   surface = get_surface();
   width = w;
   height = h;
   bufferwidth = surface.width;
   wo_menu->y = height - wo_menu->height;
   wo_menu->width = w;
   drawbg();
   bmp_draw(bmp, -offsetX, -offsetY, scale, false);
   ui_draw(ui);
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
   
   if(x < 0 || x >= info->width*scale || x >= bufferwidth || y < 0 || y >= height || y >= info->height*scale) return;

   int start_x = max(0, x);
   int end_x = min(bufferwidth, x + brush_size);
   end_x = min(end_x, info->width*scale);
   int start_y = max(0, y);
   int end_y = min(height, y + brush_size);
   end_y = min(end_y, info->height*scale);
   
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

   if(options && options->visible) {
      ui_click(ui, x, y);
      end_subroutine();
   }

   if(ui_click(ui, x, y))
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

void release(int x, int y, int window) {
   (void)window;
   prevX = -1;
   prevY = -1;

   ui_release(ui, x, y);

   end_subroutine();
}

void hover(int x, int y) {
   ui_hover(ui, x, y);

   end_subroutine();
}

void tool_click(wo_t *wo, int window) {
   (void)wo;
   (void)window;
   size++;
   char buf[4];
   uinttostr(size, buf);
   set_input_text(toolsizetext_wo, buf);
   ui_draw(ui);
}

void toolminus_click(wo_t *wo, int window) {
   (void)wo;
   (void)window;
   size--;
   if(size < 1) size = 1;
   char buf[4];
   uinttostr(size, buf);
   set_input_text(toolsizetext_wo, buf);
   ui_draw(ui);
}

void zoomout_click(wo_t *wo, int window) {
   (void)wo;
   (void)window;
   scroll_to(0, -1);
   if(scale > 1) scale--;
   char buf[4];
   sprintf(buf, "%i00%", scale);
   set_input_text(zoomtext_wo, buf);
   drawbg();
   bmp_draw((uint8_t*)bmp, 0, 0, scale, false);
   ui_draw(ui);
   set_content_height(info->height*scale + 20, -1);
}

void zoomin_click(wo_t *wo, int window) {
   (void)wo;
   (void)window;
   scroll_to(0, -1);
   scale++;
   char buf[4];
   sprintf(buf, "%i00%", scale);
   set_input_text(zoomtext_wo, buf);
   drawbg();
   bmp_draw((uint8_t*)bmp, 0, 0, scale, false);
   ui_draw(ui);
   set_content_height(info->height*scale + 20, -1);
}

void colour_callback(char *out, int window, wo_t *wo) {
   (void)window;
   (void)wo;
   colour = hextouint(out+2);
   ui_draw(ui);
}

bool bmp_check() {
   // check if supported
   if(bmp[0] != 'B' || bmp[1] != 'M') {
      dialog_msg("Error", "File isn't a BMP\n");
      return false;
   }
   if(info->bpp != 8 && info->bpp != 16) {
      dialog_msg("Error", "BPP not 8bit or 16bit\n");
      return false;
   }
   if(info->bpp == 8 && info->compressionMethod != 0) {
      dialog_msg("Error", "Compression method not BI_RGB for 8bit bmp\n");
      return false;
   }
   if(info->bpp == 16 && info->compressionMethod != 3) {
      dialog_msg("Error", "Compression method not BI_BITFIELDS for 16bit bmp\n");
      return false;
   }
   return true;
}

void load_img() {
   scroll_to(0, -1);
   // close existing
   if(current_file)
      fclose(current_file);

   FILE *f = fopen(path, "r+");
   if(!f) {
      dialog_msg("Error", "File not found\n");
      exit(0);
   }
   int size = fsize(fileno(f));
   char *buf = (char*)malloc(size);
   if(!fread(buf, size, 1, f)) {
      dialog_msg("Error", "Couldn't read file\n");
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
void open_file(char *p, int window) {
   (void)window;
   drawbg();
   strcpy(path, p);

   load_img();
   ui_draw(ui);

   set_content_height(info->height*scale + 20, -1);
}

void open_click(wo_t *wo, int window) {
   (void)wo;
   (void)window;
   dialog_filepicker("/bmp", &open_file);
}

void write_file() {
   height -= wo_menu->height; // remove menu bar
   bmp_header_t *header = (bmp_header_t*)bmp;
   uint32_t size = header->dataOffset + rowSize * info->height;
   fseek(current_file, 0, SEEK_SET);
   fwrite(bmp, size, 1, current_file);
   fflush(current_file);
   height += wo_menu->height; // restore
}

void write_callback(char *path) {
   FILE *f = fopen(path, "w");
   if(!f) {
      dialog_msg("Error", "Couldn't create file");
      debug_println("Couldn't create file %s", path);
      return;
   }
   current_file = f;
   write_file();
}

void save_click(wo_t *wo, int window) {
   (void)wo;
   (void)window;
   if(info->bpp != 16) {
      dialog_msg("Error", "Can only save 16bit bitmaps\n");
      return;
   }

   if(!current_file) {
      dialog_input("Please enter path to save image to", &write_callback);
   } else {
      write_file();
   }
}

void scroll(int deltaY, int offY, int window) {
   (void)deltaY;
   (void)window;
   offsetY = offY;
   bmp_draw((uint8_t*)bmp, -offsetX, -offsetY, scale, false);
   ui_draw(ui);
   end_subroutine();
}

void brush_callback(wo_t *wo, int index, int window) {
   (void)wo;
   (void)window;
   brush_type = index;
}

void brush_close(wo_t *wo, int window) {
   (void)window;
   wo->selected = false;
   options->visible = false;
   drawbg();
   bmp_draw((uint8_t*)bmp, -offsetX, -offsetY, scale, false);
   ui_draw(ui);
}

void brush_click(wo_t *wo, int window) {
   (void)wo;
   (void)window;

   if(options != NULL) {
      options->visible = true;
      ui_draw(ui);
      return;
   }

   // create canvas
   options = create_canvas(20, 20, 100, 120);
   wo_t *label = create_label(5, 8, 90, 14, "Brush type:");
   canvas_add(options, label);
   wo_t *menu = create_menu(5, 20, 90, 75);
   add_menu_item(menu, "Default", &brush_callback);
   add_menu_item(menu, "Circle", &brush_callback);
   add_menu_item(menu, "Square", &brush_callback);
   add_menu_item(menu, "Cross", &brush_callback);
   add_menu_item(menu, "Spray", &brush_callback);
   canvas_add(options, menu);
   wo_t *close = create_button(5, 100, 40, 14, "Close");
   set_button_release(close, &brush_close);
   canvas_add(options, close);
   ui_add(ui, options);
   ui_draw(ui);
}

// discard changes
void clear_confirm() {
   load_img(path);
   ui_draw(ui);
}

void clear_click(wo_t *wo, int window) {
   (void)wo;
   (void)window;
   dialog_yesno("Reset", "Reset image back to saved state", &clear_confirm);
}

void _start(int argc, char **args) {
   surface = get_surface();
   width = get_width();
   height = get_height();
   drawbg();

   if(argc == 1) {
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
   } else {
      // no file provided
      // create 100x100 bmp
      int width = 100;
      int height = 100;
      uint16_t colour = 0xFFFF;
      rowSize = ((width * 2 + 3) / 4) * 4;
      int fileSize = sizeof(bmp_header_t) + sizeof(bmp_info_t) + sizeof(bmp_bitmasks_t) + rowSize*height;
      bmp_header_t *header = malloc(fileSize);
      header->identifier = 0x4D42; // BM
      header->size = fileSize;
      header->dataOffset = sizeof(bmp_header_t) + sizeof(bmp_info_t) + sizeof(bmp_bitmasks_t);
      info = (bmp_info_t*)((uint8_t*)header + sizeof(bmp_header_t));
      info->headerSize = sizeof(bmp_info_t);
      info->width = width;
      info->height = height;
      info->colourPlanes = 1;
      info->bpp = 16;
      info->compressionMethod = 3;
      info->dataSize = rowSize*height;
      info->horizontalRes = 2835;
      info->verticalRes = 2835;
      bmp_bitmasks_t *masks = (bmp_bitmasks_t*)((uint8_t*)info + sizeof(bmp_info_t));
      masks->redMask   = 0xF800;
      masks->greenMask = 0x07E0;
      masks->blueMask  = 0x001F;
      bmp = (uint8_t*)header;
      bmpbuffer = (uint16_t*)(bmp + header->dataOffset);
      for(int yi = 0; yi < height; yi++) {
         uint16_t *row = bmpbuffer + yi * (rowSize / 2);
         memset16(row, colour, width);
      }
      if(bmp_check())
         bmp_draw(bmp, 0, 0, scale, false);
   }

   int index = get_free_dialog();
   dialog_t *dialog = get_dialog(index);
   dialog_init(dialog, -1);
   dialog_set_title(dialog, "BMP Viewer");

   override_click((uint32_t)&click, -1);
   override_drag((uint32_t)&click, -1);
   override_release((uint32_t)&release, -1);
   override_draw((uint32_t)NULL);
   override_resize((uint32_t)&resize, -1);
   override_hover((uint32_t)&hover, -1);

   framebuffer = (uint16_t*)(surface.buffer);
   bufferwidth = surface.width;
   ui = ui_init(&surface, -1);

   // menu
   wo_menu = create_canvas(0, height - 18, width, 18);
   ((canvas_t*)wo_menu->data)->bordered = false;
   // window objects
   int margin = 3;
   int x = margin;
   int y = 2;

   zoomoutbtn_wo = create_button(x, y, 15, 14, "-");
   set_button_release(zoomoutbtn_wo, &zoomout_click);
   canvas_add(wo_menu, zoomoutbtn_wo);
   x += zoomoutbtn_wo->width;

   zoomtext_wo = create_input(x, y, 40, 14);
   set_input_text(zoomtext_wo, "100%");
   zoomtext_wo->width = 40;
   get_input(zoomtext_wo)->halign = true;
   get_input(zoomtext_wo)->valign = true;
   canvas_add(wo_menu, zoomtext_wo);
   x += zoomtext_wo->width;

   zoominbtn_wo = create_button(x, y, 15, 14, "+");
   set_button_release(zoominbtn_wo, &zoomin_click);
   canvas_add(wo_menu, zoominbtn_wo);
   x += zoominbtn_wo->width + margin;

   openbtn_wo = create_button(x, y, 40, 14, "Open");
   set_button_release(openbtn_wo, &open_click);
   canvas_add(wo_menu, openbtn_wo);
   x += openbtn_wo->width + margin;

   savebtn_wo = create_button(x, y, 40, 14, "Save");
   set_button_release(savebtn_wo, &save_click);
   canvas_add(wo_menu, savebtn_wo);
   x += savebtn_wo->width + margin;

   wo_t *clearbtn_wo = create_button(x, y, 40, 14, "Reset");
   set_button_release(clearbtn_wo, &clear_click);
   canvas_add(wo_menu, clearbtn_wo);
   x += clearbtn_wo->width + margin;
   
   wo_t *brushtxt_wo = create_label(x, y, 45, 14, "Brush:");
   get_label(brushtxt_wo)->release_func = &brush_click;
   canvas_add(wo_menu, brushtxt_wo);
   x += brushtxt_wo->width;

   wo_t *colour_wo = dialog_create_colourbox(x, y, 14, 14, 0, -1, &colour_callback);
   canvas_add(wo_menu, colour_wo);
   x += colour_wo->width + margin;

   toolminusbtn_wo = create_button(x, y, 15, 14, "-");
   set_button_release(toolminusbtn_wo, &toolminus_click);
   canvas_add(wo_menu, toolminusbtn_wo);
   x += toolminusbtn_wo->width;

   toolsizetext_wo = create_input(x, y, 20, 14);
   set_input_text(toolsizetext_wo, "2");
   get_input(toolsizetext_wo)->halign = true;
   get_input(toolsizetext_wo)->valign = true;
   canvas_add(wo_menu, toolsizetext_wo);
   x += toolsizetext_wo->width;

   toolplusbtn_wo = create_button(x, y, 15, 14, "+");
   set_button_release(toolplusbtn_wo, &tool_click);
   canvas_add(wo_menu, toolplusbtn_wo);
   x += toolplusbtn_wo->width + margin;

   ui_add(ui, wo_menu);

   ui_draw(ui);

   redraw();

   create_scrollbar(&scroll, -1);
   set_content_height(info->height*scale + 20, -1);

   while(1==1) {
      //asm volatile("pause");
      yield();
   }

   exit(0);

}