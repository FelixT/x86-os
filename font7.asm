; 5*7 font
global font7
font7:

; size
db 75
; width
db 5
; height
db 7
; reversed
db 1
; padding
db 0
db 0
db 0

; list of characters in the order they're found
db 0
db ' '
db '-'
db '/'
db ':'
db 39 ; apostrophe
db '.'
db '>'
db '<'
db '_'
db ','
db 27 ; cursor outline
db 28 ; cursor fill
db '0'
db '1'
db '2'
db '3'
db '4'
db '5'
db '6'
db '7'
db '8'
db '9'
db 'A'
db 'B'
db 'C'
db 'D'
db 'E'
db 'F'
db 'G'
db 'H'
db 'I'
db 'J'
db 'K'
db 'L'
db 'M'
db 'N'
db 'O'
db 'P'
db 'Q'
db 'R'
db 'S'
db 'T'
db 'U'
db 'V'
db 'W'
db 'X'
db 'Y'
db 'Z'
db 'a'
db 'b'
db 'c'
db 'd'
db 'e'
db 'f'
db 'g'
db 'h'
db 'i'
db 'j'
db 'k'
db 'l'
db 'm'
db 'n'
db 'o'
db 'p'
db 'q'
db 'r'
db 's'
db 't'
db 'u'
db 'v'
db 'w'
db 'x'
db 'y'
db 'z'

global font_null
font_null:
   db 00000b
   db 00000b
   db 01010b
   db 00100b
   db 01010b
   db 00000b
   db 00000b

global font_space
font_space:
   db 00000b
   db 00000b
   db 00000b
   db 00000b
   db 00000b
   db 00000b
   db 00000b

global font_minus
font_minus:
   db 00000b
   db 00000b
   db 00000b
   db 01110b
   db 00000b
   db 00000b
   db 00000b

global font_fwdslash
font_fwdslash:
   db 00001b
   db 00010b
   db 00100b
   db 00100b
   db 00100b
   db 01000b
   db 10000b

global font_colon
font_colon:
   db 00000b
   db 00100b
   db 00100b
   db 00000b
   db 00100b
   db 00100b
   db 00000b

global font_apostrophe
font_apostrophe:
   db 00000b
   db 01000b
   db 00100b
   db 00000b
   db 00000b
   db 00000b
   db 00000b

global font_fullstop
font_fullstop:
   db 00000b
   db 00000b
   db 00000b
   db 00000b
   db 01100b
   db 01100b
   db 00000b

global font_greaterthan
font_greaterthan:
   db 00000b
   db 01000b
   db 00100b
   db 00010b
   db 00100b
   db 01000b
   db 00000b

global font_lessthan
font_lessthan:
   db 00000b
   db 00010b
   db 00100b
   db 01000b
   db 00100b
   db 00010b
   db 00000b

global font_underscore
font_underscore:
   db 00000b
   db 00000b
   db 00000b
   db 00000b
   db 00000b
   db 00000b
   db 01110b

global font_comma
font_comma:
   db 00000b
   db 00000b
   db 00000b
   db 00000b
   db 00100b
   db 01000b
   db 00000b

global font_cursor_outline
font_cursor_outline:
   db 10000b
   db 11000b
   db 10100b
   db 10010b
   db 10100b
   db 11010b
   db 00001b

global font_cursor_fill
font_cursor_fill:
   db 00000b
   db 00000b
   db 01000b
   db 01100b
   db 01000b
   db 00000b
   db 00000b

global font_0
font_0:
   db 10100b
   db 01010b
   db 11001b
   db 10101b
   db 10011b
   db 01010b
   db 00101b

global font_1
font_1:
   db 01100b
   db 10100b
   db 00100b
   db 00100b
   db 00100b
   db 00100b
   db 11111b

global font_2
font_2:
   db 01110b
   db 10001b
   db 00011b
   db 00100b
   db 01000b
   db 10000b
   db 11111b
   
global font_3
font_3:
   db 01110b
   db 10001b
   db 00001b
   db 00010b
   db 00001b
   db 10001b
   db 01110b

global font_4
font_4:
   db 00110b
   db 01010b
   db 01010b
   db 11111b
   db 00010b
   db 00010b
   db 00010b

global font_5
font_5:
   db 11111b
   db 10000b
   db 11110b
   db 00001b
   db 00001b
   db 10001b
   db 01110b

global font_6
font_6:
   db 01111b
   db 10000b
   db 11110b
   db 10001b
   db 10001b
   db 10001b
   db 01110b

global font_7
font_7:
   db 11111b
   db 00001b
   db 00010b
   db 00100b
   db 01000b
   db 10000b
   db 10000b

global font_8
font_8:
   db 01110b
   db 10001b
   db 10001b
   db 01110b
   db 10001b
   db 10001b
   db 01110b

global font_9
font_9:
   db 01110b
   db 10001b
   db 10001b
   db 01111b
   db 00001b
   db 00001b
   db 01110b

global font_A
font_A:
   db 01110b
   db 10001b
   db 11111b
   db 10001b
   db 10001b
   db 10001b
   db 10001b
   
global font_B
font_B:
   db 11110b
   db 10001b
   db 10001b
   db 11110b
   db 10001b
   db 10001b
   db 11110b

global font_C
font_C:
   db 00111b
   db 01000b
   db 10000b
   db 10000b
   db 10000b
   db 01000b
   db 00111b

global font_D
font_D:
   db 11100b
   db 10010b
   db 10001b
   db 10001b
   db 10001b
   db 10010b
   db 11100b

global font_E
font_E:
   db 11111b
   db 10000b
   db 10000b
   db 11111b
   db 10000b
   db 10000b
   db 11111b

global font_F
font_F:
   db 11111b
   db 10000b
   db 10000b
   db 11111b
   db 10000b
   db 10000b
   db 10000b

global font_G
font_G:
   db 01110b
   db 10001b
   db 10000b
   db 10000b
   db 10011b
   db 10001b
   db 01110b

global font_H
font_H:
   db 10001b
   db 10001b
   db 10001b
   db 11111b
   db 10001b
   db 10001b
   db 10001b

global font_I
font_I:
   db 11111b
   db 00100b
   db 00100b
   db 00100b
   db 00100b
   db 00100b
   db 11111b

global font_J
font_J:
   db 11111b
   db 00100b
   db 00100b
   db 00100b
   db 00100b
   db 00100b
   db 11000b

global font_K
font_K:
   db 10011b
   db 10100b
   db 11000b
   db 10000b
   db 11000b
   db 10100b
   db 10011b
   
global font_L
font_L:
   db 10000b
   db 10000b
   db 10000b
   db 10000b
   db 10000b
   db 10000b
   db 11111b

global font_M
font_M:
   db 10001b
   db 11011b
   db 10101b
   db 10001b
   db 10001b
   db 10001b
   db 10001b

global font_N
font_N:
   db 10001b
   db 10001b
   db 11001b
   db 10101b
   db 10011b
   db 10001b
   db 10001b

global font_O
font_O:
   db 00100b
   db 01010b
   db 10001b
   db 10001b
   db 10001b
   db 01010b
   db 00100b

global font_P
font_P:
   db 11110b
   db 10001b
   db 10001b
   db 11110b
   db 10000b
   db 10000b
   db 10000b

global font_Q
font_Q:
   db 00100b
   db 01010b
   db 10001b
   db 10001b
   db 10101b
   db 01010b
   db 00101b

global font_R
font_R:
   db 11110b
   db 10001b
   db 10001b
   db 11110b
   db 10100b
   db 10010b
   db 10001b

global font_S
font_S:
   db 01110b
   db 10001b
   db 10000b
   db 01110b
   db 00001b
   db 10001b
   db 01110b

global font_T
font_T:
   db 11111b
   db 00100b
   db 00100b
   db 00100b
   db 00100b
   db 00100b
   db 00100b

global font_U
font_U:
   db 10001b
   db 10001b
   db 10001b
   db 10001b
   db 10001b
   db 10001b
   db 01110b

global font_V
font_V:
   db 10001b
   db 10001b
   db 01010b
   db 01010b
   db 01010b
   db 01010b
   db 00100b

global font_W
font_W:
   db 10001b
   db 10001b
   db 10001b
   db 10101b
   db 10101b
   db 10101b
   db 01110b

global font_X
font_X:
   db 10001b
   db 01010b
   db 01010b
   db 00100b
   db 01010b
   db 01010b
   db 10001b

global font_Y
font_Y:
   db 10001b
   db 01010b
   db 00100b
   db 00100b
   db 00100b
   db 00100b
   db 00100b

global font_Z
font_Z:
   db 11111b
   db 00001b
   db 00010b
   db 00100b
   db 01000b
   db 10000b
   db 11111b

global font_a
font_a:
   db 00000b
   db 01110b
   db 00001b
   db 01111b
   db 10001b
   db 10011b
   db 01101b

global font_b
font_b:
   db 10000b
   db 10000b
   db 11110b
   db 10001b
   db 10001b
   db 10001b
   db 11110b

global font_c
font_c:
   db 00000b
   db 01110b
   db 10001b
   db 10000b
   db 10000b
   db 10001b
   db 01110b

global font_d
font_d:
   db 00001b
   db 00001b
   db 01111b
   db 10001b
   db 10001b
   db 10001b
   db 01111b

global font_e
font_e:
   db 00000b
   db 01110b
   db 10001b
   db 11111b
   db 10000b
   db 10001b
   db 01110b

global font_f
font_f:
   db 00110b
   db 01001b
   db 01000b
   db 11100b
   db 01000b
   db 01000b
   db 01000b

global font_g
font_g:
   db 00000b
   db 01111b
   db 10001b
   db 10001b
   db 01111b
   db 00001b
   db 11110b

global font_h
font_h:
   db 10000b
   db 10000b
   db 11110b
   db 10001b
   db 10001b
   db 10001b
   db 10001b

global font_i
font_i:
   db 00100b
   db 00000b
   db 01100b
   db 00100b
   db 00100b
   db 00100b
   db 01110b

global font_j
font_j:
   db 00010b
   db 00000b
   db 00110b
   db 00010b
   db 00010b
   db 10010b
   db 01100b

global font_k
font_k:
   db 10000b
   db 10000b
   db 10010b
   db 10100b
   db 11000b
   db 10100b
   db 10010b

global font_l
font_l:
   db 11000b
   db 01000b
   db 01000b
   db 01000b
   db 01000b
   db 01000b
   db 11100b

global font_m
font_m:
   db 00000b
   db 11010b
   db 10101b
   db 10101b
   db 10101b
   db 10101b
   db 10101b

global font_n
font_n:
   db 00000b
   db 11110b
   db 10001b
   db 10001b
   db 10001b
   db 10001b
   db 10001b

global font_o
font_o:
   db 00000b
   db 01110b
   db 10001b
   db 10001b
   db 10001b
   db 10001b
   db 01110b

global font_p
font_p:
   db 00000b
   db 11110b
   db 10001b
   db 10001b
   db 11110b
   db 10000b
   db 10000b

global font_q
font_q:
   db 00000b
   db 01111b
   db 10001b
   db 10001b
   db 01111b
   db 00001b
   db 00001b

global font_r
font_r:
   db 00000b
   db 10110b
   db 11001b
   db 10000b
   db 10000b
   db 10000b
   db 10000b

global font_s
font_s:
   db 00000b
   db 01110b
   db 10000b
   db 01110b
   db 00001b
   db 10001b
   db 01110b

global font_t
font_t:
   db 01000b
   db 01000b
   db 11100b
   db 01000b
   db 01000b
   db 01001b
   db 00110b

global font_u
font_u:
   db 00000b
   db 10001b
   db 10001b
   db 10001b
   db 10001b
   db 10011b
   db 01101b

global font_v
font_v:
   db 00000b
   db 10001b
   db 10001b
   db 10001b
   db 01010b
   db 01010b
   db 00100b

global font_w
font_w:
   db 00000b
   db 10001b
   db 10001b
   db 10001b
   db 10101b
   db 10101b
   db 01010b

global font_x
font_x:
   db 00000b
   db 10001b
   db 01010b
   db 00100b
   db 00100b
   db 01010b
   db 10001b

global font_y
font_y:
   db 00000b
   db 10001b
   db 10001b
   db 01111b
   db 00001b
   db 10001b
   db 01110b

global font_z
font_z:
   db 00000b
   db 11111b
   db 00010b
   db 00100b
   db 01000b
   db 10000b
   db 11111b
