#include <stdint.h>

#include "prog.h"
#include "../lib/string.h"
#include "lib/stdio.h"
#include "lib/stdlib.h"
#include "lib/dialogs.h"
#include "prog_bmp.h"
#include "lib/draw.h"
#include "lib/ui/ui_checkbox.h"

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
dialog_t *dialog = NULL;

int hover_x = -1;
int hover_y = -1;
uint16_t hover_colour;

void drawbg() {
   draw_checkeredrect(&surface, 0xBDF7, 0xDEDB, 0, 0, width, height);
}

void resize(uint32_t fb, uint32_t w, uint32_t h) {
   framebuffer = (uint16_t*)fb;

   surface = get_surface();
   *dialog->ui->surface = surface;
   width = w;
   height = h;
   bufferwidth = surface.width;
   wo_menu->y = height - wo_menu->height;
   wo_menu->width = w;
   drawbg();
   bmp_draw(bmp, -offsetX, -offsetY, scale, false);
   ui_draw(dialog->ui);
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

bool tile_pattern = false;

void set(int x, int y) {
   int brush_size = size * scale;
   if(x < 0 || x >= info->width*scale || x >= bufferwidth || y < 0 || y >= height || y >= info->height*scale) return;

   int pixel_x = x / scale;
   int pixel_y = (y + offsetY) / scale;
   
   int brush_start_x = max(0, pixel_x);
   int brush_end_x = min(info->width, pixel_x + size);
   int brush_start_y = max(0, pixel_y);
   int brush_end_y = min(info->height, pixel_y + size);
    
   for(int py = brush_start_y; py < brush_end_y; py++) {
      for(int px = brush_start_x; px < brush_end_x; px++) {
         // check pattern
         int pattern_x;
         int pattern_y;
         if(tile_pattern) {
            pattern_x = px % 8;
            pattern_y = py % 8;
         } else {
            pattern_x = ((px-pixel_x) * 8) / brush_size;
            pattern_y = ((py-pixel_y) * 8) / brush_size;
         }
            
         if(size > 1 && !(brush_patterns[brush_type][pattern_y] & (1 << (7 - pattern_x))))
            continue;
            
         // update framebuffer
         int fb_x = px * scale;
         int fb_y = py * scale - offsetY;
         if(fb_x >= 0 && fb_x < bufferwidth && fb_y >= 0 && fb_y < height) {
            for(int sy = 0; sy < scale; sy++) {
               for(int sx = 0; sx < scale; sx++) {
                  int fb_px = fb_x + sx;
                  int fb_py = fb_y + sy;
                  if(fb_px < bufferwidth && fb_py < height)
                     framebuffer[fb_px + fb_py * bufferwidth] = colour;
               }
            }
         }
         
         // update bmp
         int rowOffset = (info->height - (py + 1)) * rowSize;
         int index = rowOffset / 2 + px;
         if(index >= 0 && index < (int)info->dataSize)
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

   if(info->bpp != 16)
      end_subroutine();

   if(x > width) // clicked scrollarea
      end_subroutine();

   if(options && options->visible) {
      ui_click(dialog->ui, x, y);
      end_subroutine();
   }

   bool menu_visible = dialog->ui->default_menu->visible;
   if(ui_click(dialog->ui, x, y)) {
      if(menu_visible && !dialog->ui->default_menu->visible) {
         drawbg();
         bmp_draw(bmp, -offsetX, -offsetY, scale, false);
         ui_draw(dialog->ui);
         redraw();
      }
      end_subroutine();
   }

   if(hover_x != -1 && hover_y != -1) {
      // restore old colour
      int sz = size*scale;
      for(int py = hover_y; py < hover_y+sz; py++) {
         for(int px = hover_x; px < hover_x+sz; px++) {
            framebuffer[px + py * bufferwidth] = hover_colour;
         }
      }
      hover_x = -1;
      hover_y = -1;
   }

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

   ui_release(dialog->ui, x, y);

   end_subroutine();
}

void hover(int x, int y) {
   ui_hover(dialog->ui, x, y);

   int sz = scale*size;
   if(y >= height - wo_menu->height - sz - 2) {
      end_subroutine();
   }
   if(x >= info->width*scale || y >= info->height*scale) {
      if(hover_x != -1 && hover_y != -1) {
         // restore old colour
         for(int py = hover_y; py < hover_y+sz; py++) {
            for(int px = hover_x; px < hover_x+sz; px++) {
               framebuffer[px + py * bufferwidth] = hover_colour;
            }
         }
         hover_x = -1;
         hover_y = -1;
      }
      end_subroutine();
   }
   if(size == 1 && scale > 1 && y < height - wo_menu->height - sz - 2) {
      int img_x = x/sz;
      int img_y = y/sz;

      int start_x = img_x*sz;
      int start_y = img_y*sz;
      if(hover_x != start_x || hover_y != start_y) {
         int end_x = start_x + sz;
         int end_y = start_y + sz;
         if(hover_x != -1 && hover_y != -1) {
            // restore old colour
            for(int py = hover_y; py < hover_y+sz; py++) {
               for(int px = hover_x; px < hover_x+sz; px++) {
                  framebuffer[px + py * bufferwidth] = hover_colour;
               }
            }
         }
         hover_x = start_x;
         hover_y = start_y;
         hover_colour = framebuffer[start_x + start_y * bufferwidth];
         for(int py = start_y; py < end_y; py++) {
            for(int px = start_x; px < end_x; px++) {
               framebuffer[px + py * bufferwidth] = colour;
            }
         }
      }
   }

   end_subroutine();
}

void tool_click(wo_t *wo, int window) {
   (void)wo;
   (void)window;
   size++;
   char buf[6];
   uinttostr(size, buf);
   strcat(buf, "px");
   set_input_text(toolsizetext_wo, buf);
   ui_draw(dialog->ui);
}

void toolminus_click(wo_t *wo, int window) {
   (void)wo;
   (void)window;
   size--;
   if(size < 1) size = 1;
   char buf[6];
   uinttostr(size, buf);
   strcat(buf, "px");
   set_input_text(toolsizetext_wo, buf);
   ui_draw(dialog->ui);
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
   ui_draw(dialog->ui);
   dialog->content_height = info->height*scale + 20;
   set_content_height(dialog->content_height, -1);
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
   ui_draw(dialog->ui);
   dialog->content_height = info->height*scale + 20;
   set_content_height(dialog->content_height, -1);
}

void colour_callback(char *out, int window, wo_t *wo) {
   (void)window;
   (void)wo;
   colour = hextouint(out+2);
   ui_draw(dialog->ui);
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
   if(bmp_check()) {
      drawbg();
      bmp_draw((uint8_t*)bmp, 0, 0, scale, false);
   }

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
   ui_draw(dialog->ui);

   dialog->content_height = info->height*scale + 20;
   set_content_height(dialog->content_height, -1);
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
   dialog->ui->scrolled_y = offY;
   bmp_draw((uint8_t*)bmp, -offsetX, -offsetY, scale, false);
   ui_draw(dialog->ui);
   redraw();
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
   ui_draw(dialog->ui);
}

void brush_toggle_tile(wo_t *wo, int window) {
   (void)window;
   tile_pattern = checkbox_checked(wo);
}

void brush_click(wo_t *wo, int window) {
   (void)wo;
   (void)window;

   if(options != NULL) {
      options->visible = true;
      ui_draw(dialog->ui);
      return;
   }

   // create canvas
   options = create_canvas(20, 20, 130, 140);
   wo_t *label = create_label(5, 8, 120, 14, "Brush type:");
   canvas_add(options, label);
   wo_t *menu = create_menu(5, 20, 120, 75);
   add_menu_item(menu, "Default", &brush_callback);
   add_menu_item(menu, "Circle", &brush_callback);
   add_menu_item(menu, "Square", &brush_callback);
   add_menu_item(menu, "Cross", &brush_callback);
   add_menu_item(menu, "Spray", &brush_callback);
   canvas_add(options, menu);
   label = create_label(5, 98, 100, 20, "Tile pattern:");
   get_label(label)->bordered = false;
   canvas_add(options, label);
   wo_t *checkbox = create_checkbox(105, 98, tile_pattern);
   set_checkbox_release(checkbox, brush_toggle_tile);
   canvas_add(options, checkbox);
   wo_t *close = create_button(5, 120, 40, 14, "Close");
   set_button_release(close, &brush_close);
   canvas_add(options, close);
   ui_add(dialog->ui, options);
   ui_draw(dialog->ui);
}

// discard changes
void clear_confirm() {
   load_img();
   ui_draw(dialog->ui);
}

void clear_click(wo_t *wo, int window) {
   (void)wo;
   (void)window;
   dialog_yesno("Reset", "Reset image back to saved state", &clear_confirm);
}

void invert_colours(wo_t *wo, int index, int window) {
   (void)wo;
   (void)index;
   (void)window;
   if(info->bpp != 16) return;

   for(int y = 0; y < info->height; y++) {
      int rowOffset = (info->height - 1 - y) * rowSize;
      uint16_t *row = (uint16_t *)((uint8_t *)bmpbuffer + rowOffset);

      for(int x = 0; x < info->width; x++) {
         int index = x;
         if(rowOffset + x * 2 >= (int)info->dataSize) continue;
         uint16_t r = 255 - get_r16(row[index]);
         uint16_t g = 255 - get_g16(row[index]);
         uint16_t b = 255 - get_b16(row[index]);
         row[index] = rgb16(r, g, b);
      }
   }
}

void resize_complete(wo_t *wo, int window) {
   dialog_t *popup_dialog = dialog_from_window(window);
   wo_t *widthinput = dialog_get(popup_dialog, "widthinput");
   wo_t *heightinput = dialog_get(popup_dialog, "heightinput");
   int newwidth = strtoint(get_input(widthinput)->text);
   int newheight = strtoint(get_input(heightinput)->text);
   uint32_t newsize = newwidth*newheight;
   uint32_t oldsize = info->width*info->height*info->bpp;
   debug_println("Resizing from %i to %i", oldsize, newsize);
   bmp_header_t *header = (bmp_header_t*)bmp;
   uint16_t colour = 0xFFFF;
   int newRowSize = ((newwidth * 2 + 3) / 4) * 4;
   uint32_t oldFileSize = header->dataOffset + rowSize * info->height;
   int newFileSize = sizeof(bmp_header_t) + sizeof(bmp_info_t) + sizeof(bmp_bitmasks_t) + newRowSize*newheight;
   debug_println("File size from %i to %i", oldFileSize, newFileSize);
   bmp_header_t *newheader = malloc(newFileSize);
   // copy headers+info
   int size = min(oldFileSize, newFileSize);
   memcpy(newheader, header, size);
   uint16_t *newbmpbuffer = (uint16_t*)((uint8_t*)newheader + newheader->dataOffset);
   for(int yi = 0; yi < newheight; yi++) {
      uint16_t *row = newbmpbuffer + yi * (newRowSize / 2);
      memset16(row, colour, newwidth);
   }
   // copy existing image
   int copywidth = min(info->width, newwidth);
   int copyheight = min(info->height, newheight);
   for(int yi = 0; yi < copyheight; yi++) {
      uint16_t *row = newbmpbuffer + yi * (newRowSize / 2);
      memcpy(row, bmpbuffer + yi * (rowSize / 2), copywidth*sizeof(uint16_t));
   }

   free(header);
   bmp = (uint8_t*)newheader;
   bmpbuffer = (uint16_t*)(bmp + newheader->dataOffset);
   rowSize = newRowSize;
   info = (bmp_info_t*)((uint8_t*)newheader + sizeof(bmp_header_t));
   info->width = newwidth;
   info->height = newheight;
   dialog_close(wo, window);
   drawbg();
   bmp_draw(bmp, -offsetX, -offsetY, scale, false);
   ui_draw(dialog->ui);
   redraw();
   dialog->content_height = info->height*scale + 20;
   set_content_height(dialog->content_height, -1);
}

void resize_image(wo_t *wo, int index, int window) {
   (void)wo;
   (void)index;
   (void)window;
   dialog_t *popup_dialog = get_dialog(get_free_dialog());
   dialog_init(popup_dialog, create_window(220, 120));
   dialog_set_title(popup_dialog, "Info");

   bool editable = info->bpp == 16;

   wo_t *label = create_label(5, 5, 100, 20, "BPP: ");
   ui_add(popup_dialog->ui, label);
   char buffer[8];
   inttostr(info->bpp, buffer);
   label = create_label(110, 5, 100, 20, buffer);
   ui_add(popup_dialog->ui, label);

   label = create_label(5, 30, 100, 20, "Width: ");
   ui_add(popup_dialog->ui, label);
   wo_t *input = create_input(110, 30, 100, 20);
   get_input(input)->valign = true;
   get_input(input)->halign = true;
   input->focusable = editable;
   inttostr(info->width, buffer);
   set_input_text(input, buffer);
   ui_add(popup_dialog->ui, input);
   dialog_add(popup_dialog, "widthinput", input);

   label = create_label(5, 55, 100, 20, "Height: ");
   ui_add(popup_dialog->ui, label);
   input = create_input(110, 55, 100, 20);
   get_input(input)->valign = true;
   get_input(input)->halign = true;
   input->focusable = editable;
   inttostr(info->height, buffer);
   set_input_text(input, buffer);
   ui_add(popup_dialog->ui, input);
   dialog_add(popup_dialog, "heightinput", input);

   wo_t *button = create_button(5, 80, 100, 20, "Close");
   set_button_release(button, &dialog_close);
   ui_add(popup_dialog->ui, button);
   button = create_button(110, 80, 100, 20, "Update");
   set_button_release(button, &resize_complete);
   ui_add(popup_dialog->ui, button);

   ui_draw(popup_dialog->ui);
   redraw_w(popup_dialog->window);
}

void _start(int argc, char **args) {
   override_draw(0, -1);
   surface = get_surface();
   width = get_width();
   height = get_height();
   drawbg();

   if(argc == 2) {
      if(*args[1] != '\0') {
         if(args[1][0] != '/') {
            // relative path
            getwd(path);
            if(!strequ(path, "/"))
               strcat(path, "/");
            strcat(path, args[1]);
         } else {
            // absolute path
            strcpy(path, args[1]);
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
   dialog = get_dialog(index);
   dialog_init(dialog, -1);
   dialog_set_title(dialog, "BMP Viewer");

   override_click((uint32_t)&click, -1);
   override_drag((uint32_t)&click, -1);
   override_release((uint32_t)&release, -1);
   override_resize((uint32_t)&resize, -1);
   override_hover((uint32_t)&hover, -1);

   framebuffer = (uint16_t*)(surface.buffer);
   bufferwidth = surface.width;

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

   toolsizetext_wo = create_input(x, y, 35, 14);
   set_input_text(toolsizetext_wo, "2px");
   get_input(toolsizetext_wo)->halign = true;
   get_input(toolsizetext_wo)->valign = true;
   canvas_add(wo_menu, toolsizetext_wo);
   x += toolsizetext_wo->width;

   toolplusbtn_wo = create_button(x, y, 15, 14, "+");
   set_button_release(toolplusbtn_wo, &tool_click);
   canvas_add(wo_menu, toolplusbtn_wo);
   x += toolplusbtn_wo->width + margin;

   ui_add(dialog->ui, wo_menu);

   ui_draw(dialog->ui);

   redraw();

   create_scrollbar(&scroll, -1);
   dialog->content_height = info->height*scale + 20;
   set_content_height(dialog->content_height, -1);

   // right click menu
   strcpy(get_menu_item(dialog->ui->default_menu, 0)->text, "Quit");
   add_menu_item(dialog->ui->default_menu, "Invert colours", &invert_colours);
   add_menu_item(dialog->ui->default_menu, "Resize/Info", &resize_image);
   resize_menu(dialog->ui->default_menu);

   while(1==1) {
      //asm volatile("pause");
      yield();
   }

   exit(0);

}