void *memset(void *dst, int c, unsigned int n) {
    char *cdst = (char *)dst;
    for (unsigned int i = 0; i < n; i++) { cdst[i] = c; }
    return dst;
}
