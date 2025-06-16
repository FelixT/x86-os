#ifndef API_H
#define API_H

#include "registers_t.h"

void api_write_string(registers_t *regs);
void api_write_number(registers_t *regs);
void api_write_uint(registers_t *regs);
void api_write_newline();
void api_write_string_at(registers_t *regs);
void api_write_number_at(registers_t *regs);
void api_yield(registers_t *regs);
void api_print_program_stack(registers_t *regs);
void api_print_stack();
void api_return_framebuffer(registers_t *regs);
void api_return_window_width(registers_t *regs);
void api_return_window_height(registers_t *regs);
void api_redraw_window();
void api_redraw_pixel(registers_t *regs);
void api_end_task(registers_t *regs);
void api_override_uparrow(registers_t *regs);
void api_override_downarrow(registers_t *regs);
void api_override_mouseclick(registers_t *regs);
void api_override_draw(registers_t *regs);
void api_override_resize(registers_t *regs);
void api_override_drag(registers_t *regs);
void api_override_mouserelease(registers_t *regs);
void api_override_checkcmd(registers_t *regs);
void api_end_subroutine(registers_t *regs);
void api_malloc(registers_t *regs);
void api_free(registers_t *regs);
void api_fat_read_root(registers_t *regs);
void api_fat_get_bpb(registers_t *regs);
void api_fat_parse_path(registers_t *regs);
void api_fat_read_file(registers_t *regs);
void api_fat_write_file(registers_t *regs);
void api_fat_new_file(registers_t *regs);
void api_get_get_dir_size(registers_t *regs);
void api_read_dir(registers_t *regs);
void api_draw_bmp(registers_t *regs);
void api_clear_window(registers_t *regs);
void api_queue_event(registers_t *regs);
void api_register_windowobj(registers_t *regs);
void api_launch_task(registers_t *regs);
void api_set_sys_font(registers_t *regs);
void api_set_window_title(registers_t *regs);
void api_set_working_dir(registers_t *regs);
void api_get_working_dir(registers_t *regs);
void api_display_popup(registers_t *regs);
void api_display_colourpicker(registers_t *regs);

#endif