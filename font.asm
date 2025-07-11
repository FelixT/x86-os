
; 7*11 font

; size
db 77
; width
db 7
; height
db 11
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
db 29 ; cursor resize outline
db 30 ; cursor resize fill
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
   db 0000000b
   db 0000000b
   db 0000000b
   db 0100010b
   db 0010100b
   db 0001000b
   db 0010100b
   db 0100010b
   db 0000000b
   db 0000000b
   db 0000000b


global font_space
font_space:
   db 0000000b
   db 0000000b
   db 0000000b
   db 0000000b
   db 0000000b
   db 0000000b
   db 0000000b
   db 0000000b
   db 0000000b
   db 0000000b
   db 0000000b

global font_minus
font_minus:
   db 0000000b
   db 0000000b
   db 0000000b
   db 0000000b
   db 0000000b
   db 0111110b
   db 0000000b
   db 0000000b
   db 0000000b
   db 0000000b
   db 0000000b

global font_fwdslash
font_fwdslash:
   db 0000010b
   db 0000010b
   db 0000100b
   db 0000100b
   db 0001000b
   db 0001000b
   db 0001000b
   db 0010000b
   db 0010000b
   db 0100000b
   db 0100000b

global font_colon
font_colon:
   db 0000000b
   db 0000000b
   db 0001000b
   db 0001000b
   db 0000000b
   db 0000000b
   db 0000000b
   db 0001000b
   db 0001000b
   db 0000000b
   db 0000000b

global font_apostrophe
font_apostrophe:
   db 0000000b
   db 0010000b
   db 0001000b
   db 0000000b
   db 0000000b
   db 0000000b
   db 0000000b
   db 0000000b
   db 0000000b
   db 0000000b
   db 0000000b

global font_fullstop
font_fullstop:
   db 0000000b
   db 0000000b
   db 0000000b
   db 0000000b
   db 0000000b
   db 0000000b
   db 0000000b
   db 0000000b
   db 0011000b
   db 0011000b
   db 0000000b

global font_greaterthan
font_greaterthan:
   db 0000000b
   db 0010000b
   db 0001000b
   db 0000100b
   db 0000010b
   db 0000001b
   db 0000010b
   db 0000100b
   db 0001000b
   db 0010000b
   db 0000000b

global font_lessthan
font_lessthan:
   db 0000000b
   db 0000100b
   db 0001000b
   db 0010000b
   db 0100000b
   db 1000000b
   db 0100000b
   db 0010000b
   db 0001000b
   db 0000100b
   db 0000000b

global font_underscore
font_underscore:
   db 0000000b
   db 0000000b
   db 0000000b
   db 0000000b
   db 0000000b
   db 0000000b
   db 0000000b
   db 0000000b
   db 0000000b
   db 0000000b
   db 0111110b

global font_comma
font_comma:
   db 0000000b
   db 0000000b
   db 0000000b
   db 0000000b
   db 0000000b
   db 0000000b
   db 0000000b
   db 0000000b
   db 0010000b
   db 0100000b
   db 0000000b

global font_cursor_outline
font_cursor_outline:
   db 1100000b
   db 1010000b
   db 1001000b
   db 1000100b
   db 1000010b
   db 1000100b
   db 1000100b
   db 1010010b
   db 1101001b
   db 0000101b
   db 0000010b

global font_cursor_fill
font_cursor_fill:
   db 0000000b
   db 0100000b
   db 0110000b
   db 0111000b
   db 0111100b
   db 0111000b
   db 0111000b
   db 0101100b
   db 0000110b
   db 0000010b
   db 0000000b

; cursor resize ouline
   db 1100000b
   db 1010000b
   db 1001000b
   db 1000100b
   db 1000010b
   db 0110100b
   db 0100011b
   db 0010001b
   db 0001001b
   db 0000101b
   db 0000011b

; cursor resize fill
   db 0000000b
   db 0100000b
   db 0110000b
   db 0111000b
   db 0111100b
   db 0001000b
   db 0011110b
   db 0001110b
   db 0000110b
   db 0000010b
   db 0000000b

global font_0
font_0:
   db 0011100b
   db 0100010b
   db 1000001b
   db 1100001b
   db 1010001b
   db 1001001b
   db 1000101b
   db 1000011b
   db 1000001b
   db 0100010b
   db 0011100b
   
global font_1
font_1:
   db 0011000b
   db 0101000b
   db 1001000b
   db 0001000b
   db 0001000b
   db 0001000b
   db 0001000b
   db 0001000b
   db 0001000b
   db 0001000b
   db 1111111b

global font_2
font_2:
   db 0011100b
   db 0100010b
   db 1000001b
   db 0000001b
   db 0000010b
   db 0000100b
   db 0001000b
   db 0010000b
   db 0100000b
   db 1000000b
   db 1111111b
   
global font_3
font_3:
   db 0011100b
   db 0100010b
   db 1000001b
   db 0000001b
   db 0000010b
   db 0000100b
   db 0000010b
   db 0000001b
   db 1000001b
   db 0100010b
   db 0011100b

global font_4
font_4:
   db 0001100b
   db 0010100b
   db 0100100b
   db 0100100b
   db 1000100b
   db 1111111b
   db 0000100b
   db 0000100b
   db 0000100b
   db 0000100b
   db 0000100b

global font_5
font_5:
   db 1111111b
   db 1000000b
   db 1000000b
   db 1000000b
   db 1000000b
   db 1111110b
   db 0000001b
   db 0000001b
   db 0000001b
   db 1000001b
   db 0111110b

global font_6
font_6:
   db 0011111b
   db 0100000b
   db 1000000b
   db 1000000b
   db 1000000b
   db 1111110b
   db 1000001b
   db 1000001b
   db 1000001b
   db 1000001b
   db 0111110b

global font_7
font_7:
   db 1111111b
   db 0000001b
   db 0000010b
   db 0000100b
   db 0001000b
   db 0001000b
   db 0010000b
   db 0100000b
   db 1000000b
   db 1000000b
   db 1000000b

global font_8
font_8:
   db 0011100b
   db 0100010b
   db 1000001b
   db 1000001b
   db 0100010b
   db 0011100b
   db 0100010b
   db 1000001b
   db 1000001b
   db 0100010b
   db 0011100b

global font_9
font_9:
   db 0111110b
   db 1000001b
   db 1000001b
   db 1000001b
   db 1000001b
   db 0111111b
   db 0000001b
   db 0000001b
   db 0000001b
   db 0000001b
   db 0111110b

global font_A
font_A:
   db 0111110b
   db 1000001b
   db 1000001b
   db 1000001b
   db 1000001b
   db 1111111b
   db 1000001b
   db 1000001b
   db 1000001b
   db 1000001b
   db 1000001b
   
global font_B
font_B:
   db 1111110b
   db 1000001b
   db 1000001b
   db 1000001b
   db 1000010b
   db 1111100b
   db 1000010b
   db 1000001b
   db 1000001b
   db 1000001b
   db 1111110b

global font_C
font_C:
   db 0011110b
   db 0100001b
   db 1000000b
   db 1000000b
   db 1000000b
   db 1000000b
   db 1000000b
   db 1000000b
   db 1000000b
   db 0100001b
   db 0011110b

global font_D
font_D:
   db 1111100b
   db 1000010b
   db 1000001b
   db 1000001b
   db 1000001b
   db 1000001b
   db 1000001b
   db 1000001b
   db 1000001b
   db 1000010b
   db 1111100b

global font_E
font_E:
   db 1111111b
   db 1000000b
   db 1000000b
   db 1000000b
   db 1000000b
   db 1111111b
   db 1000000b
   db 1000000b
   db 1000000b
   db 1000000b
   db 1111111b

global font_F
font_F:
   db 1111111b
   db 1000000b
   db 1000000b
   db 1000000b
   db 1000000b
   db 1111111b
   db 1000000b
   db 1000000b
   db 1000000b
   db 1000000b
   db 1000000b

global font_G
font_G:
   db 0011111b
   db 0100000b
   db 1000000b
   db 1000000b
   db 1000000b
   db 1000000b
   db 1001111b
   db 1000001b
   db 1000001b
   db 0100001b
   db 0011111b

global font_H
font_H:
   db 1000001b
   db 1000001b
   db 1000001b
   db 1000001b
   db 1000001b
   db 1111111b
   db 1000001b
   db 1000001b
   db 1000001b
   db 1000001b
   db 1000001b

global font_I
font_I:
   db 0111110b
   db 0001000b
   db 0001000b
   db 0001000b
   db 0001000b
   db 0001000b
   db 0001000b
   db 0001000b
   db 0001000b
   db 0001000b
   db 0111110b

global font_J
font_J:
   db 1111111b
   db 0000100b
   db 0000100b
   db 0000100b
   db 0000100b
   db 0000100b
   db 0000100b
   db 0000100b
   db 0000100b
   db 0001000b
   db 1110000b

global font_K
font_K:
   db 1000001b
   db 1000010b
   db 1000100b
   db 1001000b
   db 1010000b
   db 1100000b
   db 1010000b
   db 1001000b
   db 1000100b
   db 1000010b
   db 1000001b

   
global font_L
font_L:
   db 1000000b
   db 1000000b
   db 1000000b
   db 1000000b
   db 1000000b
   db 1000000b
   db 1000000b
   db 1000000b
   db 1000000b
   db 1000000b
   db 1111111b

global font_M
font_M:
   db 1000001b
   db 1100011b
   db 1010101b
   db 1001001b
   db 1000001b
   db 1000001b
   db 1000001b
   db 1000001b
   db 1000001b
   db 1000001b
   db 1000001b

global font_N
font_N:
   db 1000001b
   db 1100001b
   db 1010001b
   db 1010001b
   db 1001001b
   db 1001001b
   db 1001001b
   db 1000101b
   db 1000101b
   db 1000011b
   db 1000001b

global font_O
font_O:
   db 0011100b
   db 0100010b
   db 1000001b
   db 1000001b
   db 1000001b
   db 1000001b
   db 1000001b
   db 1000001b
   db 1000001b
   db 0100010b
   db 0011100b

global font_P
font_P:
   db 1111110b
   db 1000001b
   db 1000001b
   db 1000001b
   db 1000001b
   db 1111110b
   db 1000000b
   db 1000000b
   db 1000000b
   db 1000000b
   db 1000000b

global font_Q
font_Q:
   db 0011100b
   db 0100010b
   db 1000001b
   db 1000001b
   db 1000001b
   db 1000001b
   db 1000001b
   db 1001001b
   db 1000101b
   db 0100010b
   db 0011101b

global font_R
font_R:
   db 1111110b
   db 1000001b
   db 1000001b
   db 1000001b
   db 1000001b
   db 1111110b
   db 1010000b
   db 1001000b
   db 1000100b
   db 1000010b
   db 1000001b

global font_S
font_S:
   db 0111111b
   db 1000000b
   db 1000000b
   db 1000000b
   db 1000000b
   db 0111110b
   db 0000001b
   db 0000001b
   db 0000001b
   db 0000001b
   db 1111110b

global font_T
font_T:
   db 1111111b
   db 0001000b
   db 0001000b
   db 0001000b
   db 0001000b
   db 0001000b
   db 0001000b
   db 0001000b
   db 0001000b
   db 0001000b
   db 0001000b

global font_U
font_U:
   db 1000001b
   db 1000001b
   db 1000001b
   db 1000001b
   db 1000001b
   db 1000001b
   db 1000001b
   db 1000001b
   db 1000001b
   db 1000001b
   db 0111110b

global font_V
font_V:
   db 1000001b
   db 1000001b
   db 1000001b
   db 0100010b
   db 0100010b
   db 0100010b
   db 0010100b
   db 0010100b
   db 0010100b
   db 0001000b
   db 0001000b

global font_W
font_W:
   db 1000001b
   db 1000001b
   db 1000001b
   db 1000001b
   db 1000001b
   db 1000001b
   db 1000001b
   db 1001001b
   db 1001001b
   db 1010101b
   db 0100010b

global font_X
font_X:
   db 1000001b
   db 1000001b
   db 0100010b
   db 0100010b
   db 0010100b
   db 0001000b
   db 0010100b
   db 0100010b
   db 0100010b
   db 1000001b
   db 1000001b

global font_Y
font_Y:
   db 1000001b
   db 0100010b
   db 0100010b
   db 0010100b
   db 0010100b
   db 0001000b
   db 0001000b
   db 0001000b
   db 0001000b
   db 0001000b
   db 0001000b

global font_Z
font_Z:
   db 1111111b
   db 0000001b
   db 0000010b
   db 0000010b
   db 0000100b
   db 0001000b
   db 0010000b
   db 0100000b
   db 0100000b
   db 1000000b
   db 1111111b

; Lowercase letters for 7x11 font

global font_a
font_a:
   db 0000000b
   db 0000000b
   db 0111110b
   db 1000001b
   db 0000001b
   db 0111111b
   db 1000001b
   db 1000001b
   db 1000001b
   db 1000011b
   db 0111101b

global font_b
font_b:
   db 1000000b
   db 1000000b
   db 1000000b
   db 1111110b
   db 1000001b
   db 1000001b
   db 1000001b
   db 1000001b
   db 1000001b
   db 1000001b
   db 1111110b

global font_c
font_c:
   db 0000000b
   db 0000000b
   db 0111110b
   db 1000001b
   db 1000000b
   db 1000000b
   db 1000000b
   db 1000000b
   db 1000000b
   db 1000001b
   db 0111110b

global font_d
font_d:
   db 0000001b
   db 0000001b
   db 0000001b
   db 0111111b
   db 1000001b
   db 1000001b
   db 1000001b
   db 1000001b
   db 1000001b
   db 1000001b
   db 0111111b

global font_e
font_e:
   db 0000000b
   db 0000000b
   db 0111110b
   db 1000001b
   db 1000001b
   db 1111111b
   db 1000000b
   db 1000000b
   db 1000000b
   db 1000001b
   db 0111110b

global font_f
font_f:
   db 0001111b
   db 0010000b
   db 0010000b
   db 1111110b
   db 0010000b
   db 0010000b
   db 0010000b
   db 0010000b
   db 0010000b
   db 0010000b
   db 0010000b

global font_g
font_g:
   db 0000000b
   db 0000000b
   db 0111111b
   db 1000001b
   db 1000001b
   db 1000001b
   db 1000001b
   db 0111111b
   db 0000001b
   db 1000001b
   db 0111110b

global font_h
font_h:
   db 1000000b
   db 1000000b
   db 1000000b
   db 1111110b
   db 1000001b
   db 1000001b
   db 1000001b
   db 1000001b
   db 1000001b
   db 1000001b
   db 1000001b

global font_i
font_i:
   db 0000000b
   db 0001000b
   db 0000000b
   db 0111000b
   db 0001000b
   db 0001000b
   db 0001000b
   db 0001000b
   db 0001000b
   db 0001000b
   db 0111110b

global font_j
font_j:
   db 0000000b
   db 0000100b
   db 0000000b
   db 0011100b
   db 0000100b
   db 0000100b
   db 0000100b
   db 0000100b
   db 0000100b
   db 1000100b
   db 0111000b

global font_k
font_k:
   db 1000000b
   db 1000000b
   db 1000000b
   db 1000010b
   db 1000100b
   db 1001000b
   db 1110000b
   db 1001000b
   db 1000100b
   db 1000010b
   db 1000001b

global font_l
font_l:
   db 0111000b
   db 0001000b
   db 0001000b
   db 0001000b
   db 0001000b
   db 0001000b
   db 0001000b
   db 0001000b
   db 0001000b
   db 0001000b
   db 0111110b

global font_m
font_m:
   db 0000000b
   db 0000000b
   db 1111110b
   db 1001001b
   db 1001001b
   db 1001001b
   db 1001001b
   db 1001001b
   db 1001001b
   db 1001001b
   db 1001001b

global font_n
font_n:
   db 0000000b
   db 0000000b
   db 1111110b
   db 1000001b
   db 1000001b
   db 1000001b
   db 1000001b
   db 1000001b
   db 1000001b
   db 1000001b
   db 1000001b

global font_o
font_o:
   db 0000000b
   db 0000000b
   db 0111110b
   db 1000001b
   db 1000001b
   db 1000001b
   db 1000001b
   db 1000001b
   db 1000001b
   db 1000001b
   db 0111110b

global font_p
font_p:
   db 0000000b
   db 0000000b
   db 1111110b
   db 1000001b
   db 1000001b
   db 1000001b
   db 1000001b
   db 1111110b
   db 1000000b
   db 1000000b
   db 1000000b

global font_q
font_q:
   db 0000000b
   db 0000000b
   db 0111111b
   db 1000001b
   db 1000001b
   db 1000001b
   db 1000001b
   db 0111111b
   db 0000001b
   db 0000001b
   db 0000001b

global font_r
font_r:
   db 0000000b
   db 0000000b
   db 1011110b
   db 1100000b
   db 1000000b
   db 1000000b
   db 1000000b
   db 1000000b
   db 1000000b
   db 1000000b
   db 1000000b

global font_s
font_s:
   db 0000000b
   db 0000000b
   db 0111111b
   db 1000000b
   db 1000000b
   db 0111110b
   db 0000001b
   db 0000001b
   db 0000001b
   db 1000001b
   db 0111110b

global font_t
font_t:
   db 0000000b
   db 0010000b
   db 0010000b
   db 1111110b
   db 0010000b
   db 0010000b
   db 0010000b
   db 0010000b
   db 0010000b
   db 0010000b
   db 0001111b

global font_u
font_u:
   db 0000000b
   db 0000000b
   db 1000001b
   db 1000001b
   db 1000001b
   db 1000001b
   db 1000001b
   db 1000001b
   db 1000001b
   db 1000011b
   db 0111101b

global font_v
font_v:
   db 0000000b
   db 0000000b
   db 1000001b
   db 1000001b
   db 0100010b
   db 0100010b
   db 0100010b
   db 0010100b
   db 0010100b
   db 0001000b
   db 0001000b

global font_w
font_w:
   db 0000000b
   db 0000000b
   db 1000001b
   db 1000001b
   db 1000001b
   db 1001001b
   db 1001001b
   db 1001001b
   db 1010101b
   db 1010101b
   db 0100010b

global font_x
font_x:
   db 0000000b
   db 0000000b
   db 1000001b
   db 0100010b
   db 0100010b
   db 0010100b
   db 0001000b
   db 0010100b
   db 0100010b
   db 1000001b
   db 1000001b

global font_y
font_y:
   db 0000000b
   db 0000000b
   db 1000001b
   db 1000001b
   db 1000001b
   db 1000001b
   db 1000001b
   db 0111111b
   db 0000001b
   db 1000001b
   db 0111110b

global font_z
font_z:
   db 0000000b
   db 0000000b
   db 1111111b
   db 0000001b
   db 0000010b
   db 0000100b
   db 0001000b
   db 0010000b
   db 0100000b
   db 1000000b
   db 1111111b