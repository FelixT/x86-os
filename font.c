#include <stdint.h>

// defines 5*7 gui font

extern uint8_t font_null[7];

extern uint8_t font_space[7];
extern uint8_t font_minus[7];
extern uint8_t font_cursor_outline[7];
extern uint8_t font_cursor_fill[7];

extern uint8_t font_0[7];
extern uint8_t font_1[7];
extern uint8_t font_2[7];
extern uint8_t font_3[7];
extern uint8_t font_4[7];
extern uint8_t font_5[7];
extern uint8_t font_6[7];
extern uint8_t font_7[7];
extern uint8_t font_8[7];
extern uint8_t font_9[7];

extern uint8_t font_A[7];
extern uint8_t font_B[7];
extern uint8_t font_C[7];
extern uint8_t font_D[7];
extern uint8_t font_E[7];
extern uint8_t font_F[7];
extern uint8_t font_G[7];
extern uint8_t font_H[7];
extern uint8_t font_I[7];
extern uint8_t font_J[7];
extern uint8_t font_K[7];
extern uint8_t font_L[7];
extern uint8_t font_M[7];
extern uint8_t font_N[7];
extern uint8_t font_O[7];
extern uint8_t font_P[7];
extern uint8_t font_Q[7];
extern uint8_t font_R[7];
extern uint8_t font_S[7];
extern uint8_t font_T[7];
extern uint8_t font_U[7];
extern uint8_t font_V[7];
extern uint8_t font_W[7];
extern uint8_t font_X[7];
extern uint8_t font_Y[7];
extern uint8_t font_Z[7];

void copyFont(uint8_t* letter, int* dest) {
   for(int y = 0; y < 7; y++) {
      //uint8_t fontrow = letter[y];

      for(int x = 0; x < 5; x++) {
         // get bit in reverse order
         int bit = (letter[y] >> (4-x)) & 1;
         dest[y*5+x] = bit;
      }
   }
}

void getFontLetter(char c, int* dest) {
   if(c == ' ')
      copyFont(font_space, dest);
   else if(c == '-')
      copyFont(font_minus, dest);
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