#ifndef API_H
#define API_H

#include "registers_t.h"

void api_write_string(registers_t *regs);
void api_write_number(registers_t *regs); // deprecated
void api_write_uint(registers_t *regs); // deprecated
void api_write_newline(); // deprecated
void api_write_string_at(registers_t *regs);
void api_write_number_at(registers_t *regs); // deprecated
void api_yield(registers_t *regs);
void api_print_program_stack(registers_t *regs);
void api_print_stack();
void api_return_framebuffer(registers_t *regs);
void api_return_window_width(registers_t *regs);
void api_return_window_height(registers_t *regs);
void api_redraw_window();
void api_redraw_pixel(registers_t *regs);
void api_end_task(registers_t *regs);
void api_override_mouseclick(registers_t *regs);
void api_override_draw(registers_t *regs);
void api_override_resize(registers_t *regs);
void api_override_drag(registers_t *regs);
void api_override_release(registers_t *regs);
void api_override_checkcmd(registers_t *regs);
void api_override_hover(registers_t *regs);
void api_end_subroutine(registers_t *regs);
void api_malloc(registers_t *regs);
void api_free(registers_t *regs);
void api_read_dir(registers_t *regs);
void api_draw_bmp(registers_t *regs);
void api_clear_window(registers_t *regs);
void api_queue_event(registers_t *regs);
void api_register_windowobj(registers_t *regs);
void api_windowobj_add_child(registers_t *regs);
void api_launch_task(registers_t *regs);
void api_set_sys_font(registers_t *regs);
void api_set_window_title(registers_t *regs);
void api_set_working_dir(registers_t *regs);
void api_get_working_dir(registers_t *regs);
void api_display_popup(registers_t *regs);
void api_display_colourpicker(registers_t *regs);
void api_display_filepicker(registers_t *regs);
void api_read(registers_t *regs);
void api_debug_write_str(registers_t *regs);
void api_sbrk(registers_t *regs);
void api_open(registers_t *regs);
void api_write(registers_t *regs);
void api_fsize(registers_t *regs);
void api_create_scrollbar(registers_t *regs);
void api_set_scrollable_height(registers_t *regs);
void api_scroll_to(registers_t *regs);
void api_new_file(registers_t *regs);
void api_mkdir(registers_t *regs);
void api_rename(registers_t *regs);
void api_set_window_size(registers_t *regs);
void api_get_font_info(registers_t *regs);
void api_create_window(registers_t *regs);
void api_close_window(registers_t *regs);
void api_override_keypress(registers_t *regs);

#endif