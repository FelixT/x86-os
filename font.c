#include <stdint.h>

#include "font.h"

// defines 5*7 gui font

font_t default_font;

extern uint8_t font_null[FONT_HEIGHT];

extern uint8_t font_space[FONT_HEIGHT];
extern uint8_t font_minus[FONT_HEIGHT];
extern uint8_t font_fwdslash[FONT_HEIGHT];
extern uint8_t font_colon[FONT_HEIGHT];
extern uint8_t font_apostrophe[FONT_HEIGHT];
extern uint8_t font_fullstop[FONT_HEIGHT];
extern uint8_t font_greaterthan[FONT_HEIGHT];
extern uint8_t font_lessthan[FONT_HEIGHT];
extern uint8_t font_underscore[FONT_HEIGHT];
extern uint8_t font_comma[FONT_HEIGHT];

extern uint8_t font_cursor_outline[FONT_HEIGHT];
extern uint8_t font_cursor_fill[FONT_HEIGHT];

extern uint8_t font_0[FONT_HEIGHT];
extern uint8_t font_1[FONT_HEIGHT];
extern uint8_t font_2[FONT_HEIGHT];
extern uint8_t font_3[FONT_HEIGHT];
extern uint8_t font_4[FONT_HEIGHT];
extern uint8_t font_5[FONT_HEIGHT];
extern uint8_t font_6[FONT_HEIGHT];
extern uint8_t font_7[FONT_HEIGHT];
extern uint8_t font_8[FONT_HEIGHT];
extern uint8_t font_9[FONT_HEIGHT];

extern uint8_t font_A[FONT_HEIGHT];
extern uint8_t font_B[FONT_HEIGHT];
extern uint8_t font_C[FONT_HEIGHT];
extern uint8_t font_D[FONT_HEIGHT];
extern uint8_t font_E[FONT_HEIGHT];
extern uint8_t font_F[FONT_HEIGHT];
extern uint8_t font_G[FONT_HEIGHT];
extern uint8_t font_H[FONT_HEIGHT];
extern uint8_t font_I[FONT_HEIGHT];
extern uint8_t font_J[FONT_HEIGHT];
extern uint8_t font_K[FONT_HEIGHT];
extern uint8_t font_L[FONT_HEIGHT];
extern uint8_t font_M[FONT_HEIGHT];
extern uint8_t font_N[FONT_HEIGHT];
extern uint8_t font_O[FONT_HEIGHT];
extern uint8_t font_P[FONT_HEIGHT];
extern uint8_t font_Q[FONT_HEIGHT];
extern uint8_t font_R[FONT_HEIGHT];
extern uint8_t font_S[FONT_HEIGHT];
extern uint8_t font_T[FONT_HEIGHT];
extern uint8_t font_U[FONT_HEIGHT];
extern uint8_t font_V[FONT_HEIGHT];
extern uint8_t font_W[FONT_HEIGHT];
extern uint8_t font_X[FONT_HEIGHT];
extern uint8_t font_Y[FONT_HEIGHT];
extern uint8_t font_Z[FONT_HEIGHT];

void copyFont(font_t *font, uint8_t* letter, int* dest) {
   for(int y = 0; y < font->height; y++) {
      //uint8_t fontrow = letter[y];

      for(int x = 0; x < font->width; x++) {
         // get bit in reverse order
         int bit = (letter[y] >> ((font->width-1)-x)) & 1;
         dest[y*font->width+x] = bit;
      }
   }
}

void getFontLetter(font_t *font, char c, int* dest) {
   if(c == ' ')
      copyFont(font, font->font_space, dest);
   else if(c == '-')
      copyFont(font, font->font_minus, dest);
   else if(c == '/')
      copyFont(font, font->font_fwdslash, dest);
   else if(c == ':')
      copyFont(font, font->font_colon, dest);
   else if(c == '\'')
      copyFont(font, font->font_apostrophe, dest);
   else if(c == '.')
      copyFont(font, font->font_fullstop, dest);
   else if(c == '>')
      copyFont(font, font->font_greaterthan, dest);
   else if(c == '<')
      copyFont(font, font->font_lessthan, dest);
   else if(c == '_')
      copyFont(font, font->font_underscore, dest);
   else if(c == ',')
      copyFont(font, font->font_comma, dest);
   else if(c == 27) // unused control characters used for cursor
      copyFont(font, font->font_cursor_outline, dest);
   else if(c == 28) // unused control characters used for cursor
      copyFont(font, font->font_cursor_fill, dest);
   else if(c == 'A')
      copyFont(font, font->font_A, dest);
   else if(c == 'B')
      copyFont(font, font->font_B, dest);
   else if(c == 'C')
      copyFont(font, font->font_C, dest);
   else if(c == 'D')
      copyFont(font, font->font_D, dest);
   else if(c == 'E')
      copyFont(font, font->font_E, dest);
   else if(c == 'F')
      copyFont(font, font->font_F, dest);
   else if(c == 'G')
      copyFont(font, font->font_G, dest);
   else if(c == 'H')
      copyFont(font, font->font_H, dest);
   else if(c == 'I')
      copyFont(font, font->font_I, dest);
   else if(c == 'J')
      copyFont(font, font->font_J, dest);
   else if(c == 'K')
      copyFont(font, font->font_K, dest);
   else if(c == 'L')
      copyFont(font, font->font_L, dest);
   else if(c == 'M')
      copyFont(font, font->font_M, dest);
   else if(c == 'N')
      copyFont(font, font->font_N, dest);
   else if(c == 'O')
      copyFont(font, font->font_O, dest);
   else if(c == 'P')
      copyFont(font, font->font_P, dest);
   else if(c == 'Q')
      copyFont(font, font->font_Q, dest);
   else if(c == 'R')
      copyFont(font, font->font_R, dest);
   else if(c == 'S')
      copyFont(font, font->font_S, dest);
   else if(c == 'T')
      copyFont(font, font->font_T, dest);
   else if(c == 'U')
      copyFont(font, font->font_U, dest);
   else if(c == 'V')
      copyFont(font, font->font_V, dest);
   else if(c == 'W')
      copyFont(font, font->font_W, dest);
   else if(c == 'X')
      copyFont(font, font->font_X, dest);
   else if(c == 'Y')
      copyFont(font, font->font_Y, dest);
   else if(c == 'Z')
      copyFont(font, font->font_Z, dest);
   else if(c == '0')
      copyFont(font, font->font_0, dest);
   else if(c == '1')
      copyFont(font, font->font_1, dest);
   else if(c == '2')
      copyFont(font, font->font_2, dest);
   else if(c == '3')
      copyFont(font, font->font_3, dest);
   else if(c == '4')
      copyFont(font, font->font_4, dest);
   else if(c == '5')
      copyFont(font, font->font_5, dest);
   else if(c == '6')
      copyFont(font, font->font_6, dest);
   else if(c == '7')
      copyFont(font, font->font_7, dest);
   else if(c == '8')
      copyFont(font, font->font_8, dest);
   else if(c == '9')
      copyFont(font, font->font_9, dest);
   else
      copyFont(font, font->font_null, dest);
}

font_t *getFont() {
   return &default_font;
}

void font_init() {

   default_font.width = 5;
   default_font.height = 7;
   default_font.padding = 2;
   default_font.font_null = (uint8_t*)font_null;

   default_font.font_space = (uint8_t*)font_space;
   default_font.font_minus = (uint8_t*)font_minus;
   default_font.font_fwdslash = (uint8_t*)font_fwdslash;
   default_font.font_colon = (uint8_t*)font_colon;
   default_font.font_apostrophe = (uint8_t*)font_apostrophe;
   default_font.font_fullstop = (uint8_t*)font_fullstop;
   default_font.font_greaterthan = (uint8_t*)font_greaterthan;
   default_font.font_lessthan = (uint8_t*)font_lessthan;
   default_font.font_underscore = (uint8_t*)font_underscore;
   default_font.font_comma = (uint8_t*)font_comma;

   default_font.font_cursor_outline = (uint8_t*)font_cursor_outline;
   default_font.font_cursor_fill = (uint8_t*)font_cursor_fill;

   default_font.font_0 = (uint8_t*)font_0;
   default_font.font_1 = (uint8_t*)font_1;
   default_font.font_2 = (uint8_t*)font_2;
   default_font.font_3 = (uint8_t*)font_3;
   default_font.font_4 = (uint8_t*)font_4;
   default_font.font_5 = (uint8_t*)font_5;
   default_font.font_6 = (uint8_t*)font_6;
   default_font.font_7 = (uint8_t*)font_7;
   default_font.font_8 = (uint8_t*)font_8;
   default_font.font_9 = (uint8_t*)font_9;

   default_font.font_A = (uint8_t*)font_A;
   default_font.font_B = (uint8_t*)font_B;
   default_font.font_C = (uint8_t*)font_C;
   default_font.font_D = (uint8_t*)font_D;
   default_font.font_E = (uint8_t*)font_E;
   default_font.font_F = (uint8_t*)font_F;
   default_font.font_G = (uint8_t*)font_G;
   default_font.font_H = (uint8_t*)font_H;
   default_font.font_I = (uint8_t*)font_I;
   default_font.font_J = (uint8_t*)font_J;
   default_font.font_K = (uint8_t*)font_K;
   default_font.font_L = (uint8_t*)font_L;
   default_font.font_M = (uint8_t*)font_M;
   default_font.font_N = (uint8_t*)font_N;
   default_font.font_O = (uint8_t*)font_O;
   default_font.font_P = (uint8_t*)font_P;
   default_font.font_Q = (uint8_t*)font_Q;
   default_font.font_R = (uint8_t*)font_R;
   default_font.font_S = (uint8_t*)font_S;
   default_font.font_T = (uint8_t*)font_T;
   default_font.font_U = (uint8_t*)font_U;
   default_font.font_V = (uint8_t*)font_V;
   default_font.font_W = (uint8_t*)font_W;
   default_font.font_X = (uint8_t*)font_X;
   default_font.font_Y = (uint8_t*)font_Y;
   default_font.font_Z = (uint8_t*)font_Z;
}