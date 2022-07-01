#include <stdint.h>

// defines 5*7 gui font

#define FONT_WIDTH 7
#define FONT_HEIGHT 11

extern uint8_t font_null[FONT_HEIGHT];

extern uint8_t font_space[FONT_HEIGHT];
extern uint8_t font_minus[FONT_HEIGHT];
extern uint8_t font_fwdslash[FONT_HEIGHT];
extern uint8_t font_colon[FONT_HEIGHT];
extern uint8_t font_apostrophe[FONT_HEIGHT];
extern uint8_t font_fullstop[FONT_HEIGHT];
extern uint8_t font_greaterthan[FONT_HEIGHT];

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

void copyFont(uint8_t* letter, int* dest) {
   for(int y = 0; y < FONT_HEIGHT; y++) {
      //uint8_t fontrow = letter[y];

      for(int x = 0; x < FONT_WIDTH; x++) {
         // get bit in reverse order
         int bit = (letter[y] >> ((FONT_WIDTH-1)-x)) & 1;
         dest[y*FONT_WIDTH+x] = bit;
      }
   }
}

void getFontLetter(char c, int* dest) {
   if(c == ' ')
      copyFont(font_space, dest);
   else if(c == '-')
      copyFont(font_minus, dest);
   else if(c == '/')
      copyFont(font_fwdslash, dest);
   else if(c == ':')
      copyFont(font_colon, dest);
   else if(c == '\'')
      copyFont(font_apostrophe, dest);
   else if(c == '.')
      copyFont(font_fullstop, dest);
   else if(c == '>')
      copyFont(font_greaterthan, dest);
   else if(c == 27) // unused control characters used for cursor
      copyFont(font_cursor_outline, dest);
   else if(c == 28) // unused control characters used for cursor
      copyFont(font_cursor_fill, dest);
   else if(c == 'A')
      copyFont(font_A, dest);
   else if(c == 'B')
      copyFont(font_B, dest);
   else if(c == 'C')
      copyFont(font_C, dest);
   else if(c == 'D')
      copyFont(font_D, dest);
   else if(c == 'E')
      copyFont(font_E, dest);
   else if(c == 'F')
      copyFont(font_F, dest);
   else if(c == 'G')
      copyFont(font_G, dest);
   else if(c == 'H')
      copyFont(font_H, dest);
   else if(c == 'I')
      copyFont(font_I, dest);
   else if(c == 'J')
      copyFont(font_J, dest);
   else if(c == 'K')
      copyFont(font_K, dest);
   else if(c == 'L')
      copyFont(font_L, dest);
   else if(c == 'M')
      copyFont(font_M, dest);
   else if(c == 'N')
      copyFont(font_N, dest);
   else if(c == 'O')
      copyFont(font_O, dest);
   else if(c == 'P')
      copyFont(font_P, dest);
   else if(c == 'Q')
      copyFont(font_Q, dest);
   else if(c == 'R')
      copyFont(font_R, dest);
   else if(c == 'S')
      copyFont(font_S, dest);
   else if(c == 'T')
      copyFont(font_T, dest);
   else if(c == 'U')
      copyFont(font_U, dest);
   else if(c == 'V')
      copyFont(font_V, dest);
   else if(c == 'W')
      copyFont(font_W, dest);
   else if(c == 'X')
      copyFont(font_X, dest);
   else if(c == 'Y')
      copyFont(font_Y, dest);
   else if(c == 'Z')
      copyFont(font_Z, dest);
   else if(c == '0')
      copyFont(font_0, dest);
   else if(c == '1')
      copyFont(font_1, dest);
   else if(c == '2')
      copyFont(font_2, dest);
   else if(c == '3')
      copyFont(font_3, dest);
   else if(c == '4')
      copyFont(font_4, dest);
   else if(c == '5')
      copyFont(font_5, dest);
   else if(c == '6')
      copyFont(font_6, dest);
   else if(c == '7')
      copyFont(font_7, dest);
   else if(c == '8')
      copyFont(font_8, dest);
   else if(c == '9')
      copyFont(font_9, dest);
   else
      copyFont(font_null, dest);
}