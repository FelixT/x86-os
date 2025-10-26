#include "sort.h"

#include "stdio.h"

// quicksort

void swap(void* a, void* b, int size) {
   unsigned char temp[size];
   memcpy(temp, a, size);
   memcpy(a, b, size);
   memcpy(b, temp, size);
}

int partition(void *arr, int low, int high, int size, int (*cmp)(const void*, const void*)) {
   // use middle element as pivot
   int mid = low + (high - low) / 2;
   
   unsigned char pivot[size];
   memcpy(pivot, arr + mid * size, size);

   int i = low - 1;
   int j = high + 1;
   
   while (1) {
      // Find element >= pivot from left
      do {
         i++;
      } while (cmp(arr + i * size, pivot) < 0);
      
      // Find element <= pivot from right
      do {
         j--;
      } while (cmp(arr + j * size, pivot) > 0);
      
      // If pointers crossed, we're done
      if (i >= j)
         return j;
      
      // Swap elements at i and j
      swap(arr + i * size, arr + j * size, size);
   }
}

void quicksort(void *arr, int low, int high, int size, int (*cmp)(const void*, const void*)) {
   if (low < high) {
      int pi = partition(arr, low, high, size, cmp);
        
      quicksort(arr, low, pi, size, cmp);
      quicksort(arr, pi + 1, high, size, cmp);
   }
}

int cmp_int(const void* a, const void* b) {
   int arg1 = *(const int*)a;
   int arg2 = *(const int*)b;
   return (arg1 > arg2) - (arg1 < arg2);
}

int cmp_str(const void* a, const void* b) {
   const char* str1 = *(const char**)a;
   const char* str2 = *(const char**)b;
   return strcmp(str1, str2);
}

void sort(void *arr, int n, int size, int (*cmp)(const void*, const void*)) {
   quicksort(arr, 0, n - 1, size, cmp);
}

void sorti(int *arr, int n) {
   sort(arr, n, sizeof(int), &cmp_int);
}

void sortstr(char **arr, int n) {
   sort(arr, n, sizeof(char*), &cmp_str);
}

int strcmp(const char* s1, const char* s2) {
   while (*s1 && (*s1 == *s2)) {
      s1++;
      s2++;
   }
   return *(unsigned char*)s1 - *(unsigned char*)s2;
}
