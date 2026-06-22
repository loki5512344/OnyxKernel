#ifndef SLIPPER_STRING_H
#define SLIPPER_STRING_H

#include "types.h"

int  strcmp(const char *a, const char *b);
int  strncmp(const char *a, const char *b, usize n);
usize strlen(const char *s);
void *memcpy(void *dst, const void *src, usize n);
void *memset(void *s, int c, usize n);
void *memmove(void *dst, const void *src, usize n);

#endif
