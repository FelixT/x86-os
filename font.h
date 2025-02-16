#ifndef FONT_H
#define FONT_H

#define FONT_WIDTH 5
#define FONT_HEIGHT 7
#define FONT_PADDING 2

typedef struct font_t {
   int width;
   int height;
   int padding;
   uint8_t *font_null;
   uint8_t *font_space;
   uint8_t *font_minus;
   uint8_t *font_fwdslash;
   uint8_t *font_colon;
   uint8_t *font_apostrophe;
   uint8_t *font_fullstop;
   uint8_t *font_greaterthan;
   uint8_t *font_lessthan;
   uint8_t *font_underscore;
   uint8_t *font_comma;
   uint8_t *font_cursor_outline;
   uint8_t *font_cursor_fill;
   uint8_t *font_0;
   uint8_t *font_1;
   uint8_t *font_2;
   uint8_t *font_3;
   uint8_t *font_4;
   uint8_t *font_5;
   uint8_t *font_6;
   uint8_t *font_7;
   uint8_t *font_8;
   uint8_t *font_9;
   uint8_t *font_A;
   uint8_t *font_B;
   uint8_t *font_C;
   uint8_t *font_D;
   uint8_t *font_E;
   uint8_t *font_F;
   uint8_t *font_G;
   uint8_t *font_H;
   uint8_t *font_I;
   uint8_t *font_J;
   uint8_t *font_K;
   uint8_t *font_L;
   uint8_t *font_M;
   uint8_t *font_N;
   uint8_t *font_O;
   uint8_t *font_P;
   uint8_t *font_Q;
   uint8_t *font_R;
   uint8_t *font_S;
   uint8_t *font_T;
   uint8_t *font_U;
   uint8_t *font_V;
   uint8_t *font_W;
   uint8_t *font_X;
   uint8_t *font_Y;
   uint8_t *font_Z;
} font_t;

font_t *getFont();
void getFontLetter(font_t *font, char c, int* dest);
void font_init();

#endif