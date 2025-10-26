#ifndef SORT_H
#define SORT_H

void sort(void *arr, int n, int size, int (*cmp)(const void*, const void*));
void sorti(int *arr, int n);
void sortstr(char **arr, int n);
int strcmp(const char* s1, const char* s2);

#endif