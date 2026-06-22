#include "string.h"

int strcmp(const char *a, const char *b)
{
    while (*a && *a == *b) { a++; b++; }
    return (unsigned char)*a - (unsigned char)*b;
}

int strncmp(const char *a, const char *b, usize n)
{
    for (usize i = 0; i < n; i++) {
        if (a[i] != b[i]) return (unsigned char)a[i] - (unsigned char)b[i];
        if (!a[i]) return 0;
    }
    return 0;
}

usize strlen(const char *s)
{
    usize n = 0;
    while (*s++) n++;
    return n;
}

void *memcpy(void *dst, const void *src, usize n)
{
    char *d = dst;
    const char *s = src;
    for (usize i = 0; i < n; i++) d[i] = s[i];
    return dst;
}

void *memset(void *s, int c, usize n)
{
    unsigned char *p = s;
    for (usize i = 0; i < n; i++) p[i] = (unsigned char)c;
    return s;
}

void *memmove(void *dst, const void *src, usize n)
{
    char *d = dst;
    const char *s = src;
    if (d < s) {
        for (usize i = 0; i < n; i++) d[i] = s[i];
    } else if (d > s) {
        for (usize i = n; i > 0; i--) d[i-1] = s[i-1];
    }
    return dst;
}
