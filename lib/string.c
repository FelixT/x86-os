#include <stddef.h>
#include <stdarg.h>
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

void strncpy(char* dest, const char* src, size_t size) {
   int i = 0;

   while((size_t)i < size && src[i] != '\0') {
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
      if(src[i] == '\0') {
         if(dest1 != NULL)
            dest1[i] = '\0';
         if(dest2 != NULL)
            dest2[0] = '\0';
         return false;
      }

      if(dest1 != NULL)
         dest1[i] = src[i];
      i++;
   }
   if(dest1 != NULL)
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


// split at last of char
bool strsplit_last(char* dest1, char* dest2, char* src, char splitat) {
   int at = -1;
   int len = strlen(src);
   for(int i = 0; i < len; i++) {
      if(src[i] == splitat) {
         at = i;
      }
   }

   if(at == -1) {
      // not found
      if(dest1 != NULL)
         strcpy(dest1, src);
      if(dest2 != NULL)
         dest2[0] = '\0';
      return false;
   }

   if(dest1 != NULL)
      strcpy_fixed(dest1, src, at);

   if(dest2 != NULL)
      strcpy_fixed(dest2, src + at + 1, len - (at + 1));

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

bool strequ(char* str1, char* str2) {
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
   if(negative)
      length++; // for the negative sign
   
   out[length] = '\0';

   for(int i = 0; i < length; i++) {
      out[length-i-1] = '0' + num%10;
      num/=10;
   }

   if(negative)
      out[0] = '-';
}

int strtoint(char *str) {
   int result = 0;
   int sign = 1;
   
   // skip whitespace
   while (*str == ' ') str++;
   
   if(*str == '-') {
      sign = -1;
      str++;
   } else if(*str == '+') {
      str++;
   }
   
   while(*str >= '0' && *str <= '9') {
      result = result * 10 + (*str - '0');
      str++;
   }
   
   return sign * result;
}

char *strcat(char *dest, const char *src) {
   char *ptr = dest;
   while(*ptr) ptr++;
   while(*src) *ptr++ = *src++;
   *ptr = '\0';
   return dest;
}

void vsnprintf(char *buffer, size_t size, char *format, va_list args) {
   char *pfmt = format;
   char x[2] = "x";

   buffer[0] = '\0';

   int i = 0;
   while((size_t)i < size && *pfmt) {
      if(*pfmt == '%' && *(pfmt + 1)) {
         pfmt++;
         switch (*pfmt) {
            case 'i':
               int i = va_arg(args, int);
               char istr[20];
               inttostr(i, istr);
               strcat(buffer, istr);
               break;
            case 'u':
               uint32_t u = va_arg(args, uint32_t);
               char ustr[20];
               uinttostr(u, ustr);
               strcat(buffer, ustr);
               break;
            case 'h':
               uint32_t h = va_arg(args, uint32_t);
               char hstr[20];
               uinttohexstr(h, hstr);
               strcat(buffer, hstr);
               break;
            case 's':
               char *s = va_arg(args, char *);
               strcat(buffer, s);
               break;
            case 'c':
               x[0] = (char)va_arg(args, int);
               strcat(buffer, x);
               break;
            default:
               x[0] = '%';
               strcat(buffer, x);
               x[0] = *pfmt;
               strcat(buffer, pfmt);
         }
      } else {
         x[0] = *pfmt;
         strcat(buffer, x);
      }
      pfmt++;
      i++;
   }

}

void sprintf(char *buffer, char *format, ...) {
   va_list args;
   va_start(args, format);
   vsnprintf(buffer, INT32_MAX, format, args);
   va_end(args);
}

void snprintf(char *buffer, size_t size, char *format, ...) {
   va_list args;
   va_start(args, format);
   vsnprintf(buffer, size, format, args);
   va_end(args);
}

char *strchr(const char *str, int c) {
   while(*str) {
      if(*str == (char)c)
         return (char*)str;
      str++;
   }
   return NULL;
}

uint32_t hextouint(char *str) {
   uint32_t u = 0;

   while(*str != '\0') {
      if(*str >= '0' && *str <= '9')
         u = (u << 4) + (*str - '0');
      else if(*str >= 'A' && *str <= 'F')
         u = (u << 4) + (*str - 'A' + 10);
      else if(*str >= 'a' && *str <= 'f')
         u = (u << 4) + (*str - 'a' + 10);
      str++;
   }
   return u;
}

void memset(void *dest, uint8_t ch, int count) {
   // faster memset using fancy assembly
   // Use stosb for small blocks (under 16 bytes)
   if(count < 16) {
      asm volatile (
         "rep stosb"
         :
         : "D" (dest), "a" (ch), "c" (count)
         : "memory", "cc"
      );
      return;
   }
    
   // For larger blocks, use stosd (4 bytes at a time)
   uint32_t value = ch;
   value |= value << 8;
   value |= value << 16;  // Replicate the byte across all 4 bytes
   
   // Handle unaligned prefix bytes
   int pre = (4 - ((uintptr_t)dest & 3)) & 3;
   if(pre > 0) {
      if(pre > count) pre = count;
      asm volatile (
         "rep stosb"
         :
         : "D" (dest), "a" (ch), "c" (pre)
         : "memory", "cc"
      );
      dest = (char*)dest + pre;
      count -= pre;
   }
   
   // Handle the bulk with stosd (4 bytes at a time)
   size_t dwords = count / 4;
   if(dwords > 0) {
      asm volatile (
         "rep stosl"
         :
         : "D" (dest), "a" (value), "c" (dwords)
         : "memory", "cc"
      );
      dest = (char*)dest + (dwords * 4);
      count &= 3;
   }
   
   // Handle trailing bytes
   if(count > 0) {
      asm volatile (
         "rep stosb" :
         : "D" (dest), "a" (ch), "c" (count)
         : "memory", "cc"
      );
   }

}
