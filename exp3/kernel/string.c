#include "types.h"

void memset(void *dst, int c, uint n) {
    uchar *d = (uchar*)dst;
    for(uint i=0; i<n; i++) d[i] = c;
}

void memcpy(void *dst, const void *src, uint n) {
    uchar *d = (uchar*)dst;
    const uchar *s = (const uchar*)src;
    for(uint i=0; i<n; i++) d[i] = s[i];
}
