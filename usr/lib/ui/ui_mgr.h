#ifndef UI_MGR_H
#define UI_MGR_H

typedef struct ui_mgr_t ui_mgr_t;

#include "../../prog.h"
#include "wo.h"

#define MAX_WO 64

typedef struct ui_mgr_t {
   surface_t *surface;
   int window;
   wo_t* wos[MAX_WO];
   int wo_count;
   
   wo_t *focused;
   wo_t *hovered;
   wo_t *clicked;
   wo_t *default_menu;
   int scrolled_y;
} ui_mgr_t;

ui_mgr_t *ui_init(surface_t *surface, int window);
void ui_destroy(ui_mgr_t *ui);
int ui_add(ui_mgr_t *ui, wo_t *wo);
void ui_remove(ui_mgr_t *ui, wo_t *wo);
void ui_draw(ui_mgr_t *ui);
void ui_redraw(ui_mgr_t *ui);
void ui_click(ui_mgr_t *ui, int x, int y);
void ui_release(ui_mgr_t *ui, int x, int y);
void ui_keypress(ui_mgr_t *ui, uint16_t c);
void ui_hover(ui_mgr_t *ui, int x, int y);
void ui_rightclick(ui_mgr_t *ui, int x, int y);
void ui_scroll(ui_mgr_t *ui, int deltaY, int offsetY);

#endif