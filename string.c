#include <stddef.h>
#include "string.h"

void strtoupper(char* dest, char* src) {
   int i = 0;

   while(src[i] != '\0') {
      if(src[i] >= 'a' && src[i] <= 'z')
         dest[i] = src[i] + ('A' - 'a');
      else
         dest[i] = src[i];
      i++;
   }
   dest[i] = '\0';
}

void strcpy(char* dest, char* src) {
   int i = 0;

   while(src[i] != '\0') {
      dest[i] = src[i];
      i++;
   }
   dest[i] = '\0';
}

void strcpy_fixed(char* dest, char* src, int length) {
   for(int i = 0; i < length; i++)
      dest[i] = src[i];
   dest[length] = '\0';
}

int stoi(char* str) {
   // convert str to int
   int out = 0;
   int power = 1;
   for(int i = strlen(str) - 1; i >= 0 ; i--) {
      if(str[i] >= '0' && str[i] <= '9') {
         out += power*(str[i]-'0');
         power *= 10;
      }
   }
   return out;
}

// split at first of char
bool strsplit(char* dest1, char* dest2, char* src, char splitat) {
   int i = 0;

   while(src[i] != splitat) {
      if(src[i] == '\0')
         return false;

      if(dest1 != NULL)
         dest1[i] = src[i];
      i++;
   }
   dest1[i] = '\0';
   i++;

   int start = i;
   while(src[i] != '\0') {
      if(dest2 != NULL)
         dest2[i - start] = src[i];
      i++;
   }
   if(dest2 != NULL)
      dest2[i - start] = '\0';

   return true;
}

bool strstartswith(char* src, char* startswith) {
   int i = 0;
   while(startswith[i] != '\0') {
      if(src[i] != startswith[i])
         return false;
      i++;
   }
   return true;
}

int strlen(char* str) {
   int len = 0;
   while(str[len] != '\0')
      len++;
   return len;
}

bool strcmp(char* str1, char* str2) {
   int len = strlen(str1);
   if(len != strlen(str2))
      return false;

   for(int i = 0; i < len; i++)
      if(str1[i] != str2[i])
         return false;

   return true;
}

void uinttohexstr(uint32_t num, char* out) {
   if(num == 0) {
      out[0] = '0';
      out[1] = '\0';
      return;
   }

   // get number length in digits
   uint32_t tmp = num;
   int length = 0;
   while(tmp > 0) {
      length++;
      tmp/=16;
   }
   
   out[length] = '\0';

   for(int i = 0; i < length; i++) {
      if(num%16 == 10) out[length-i-1] = 'A';
      else if(num%16 == 11) out[length-i-1] = 'B';
      else if(num%16 == 12) out[length-i-1] = 'C';
      else if(num%16 == 13) out[length-i-1] = 'D';
      else if(num%16 == 14) out[length-i-1] = 'E';
      else if(num%16 == 15) out[length-i-1] = 'F';
      else out[length-i-1] = '0' + num%16;
      num/=16;
   }
}

void uinttostr(uint32_t num, char* out) {
   if(num == 0) {
      out[0] = '0';
      out[1] = '\0';
      return;
   }

   // get number length in digits
   uint32_t tmp = num;
   int length = 0;
   while(tmp > 0) {
      length++;
      tmp/=10;
   }
   
   out[length] = '\0';

   for(int i = 0; i < length; i++) {
      out[length-i-1] = '0' + num%10;
      num/=10;
   }
}

void inttostr(int num, char* out) {
   if(num == 0) {
      out[0] = '0';
      out[1] = '\0';
      return;
   }

   bool negative = num < 0;
   if(negative)
      num = -num;

   // get number length in digits
   int tmp = num;
   int length = 0;
   while(tmp > 0) {
      length++;
      tmp/=10;
   }
   
   out[length] = '\0';

   for(int i = 0; i < length; i++) {
      out[length-i-1] = '0' + num%10;
      num/=10;
   }
}
