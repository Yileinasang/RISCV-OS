void *memset(void *dst, int c, unsigned int n) {
    char *cdst = (char *)dst;
    int i;
    for (i = 0; i < n; i++) {
        cdst[i] = c;
    }
    return dst;
}

void *memmove(void *dst, const void *src, unsigned int n) {
    const char *s;
    char *d;

    if (n == 0)
        return dst;

    s = src;
    d = dst;
    if (s < d && s + n > d) {
        s += n;
        d += n;
        while (n-- > 0)
            *--d = *--s;
    }
    else
        while (n-- > 0)
            *d++ = *s++;

    return dst;
}

int strncmp(const char *p, const char *q, unsigned int n) {
    while (n > 0 && *p && *p == *q)
        n--, p++, q++;
    if (n == 0)
        return 0;
    return (unsigned char)*p - (unsigned char)*q;
}

char *strncpy(char *s, const char *t, int n) {
    char *os;

    os = s;
    while (n-- > 0 && (*s++ = *t++) != 0);
    while (n-- > 0)
        *s++ = 0;
    return os;
}

int strlen(const char *s) {
    int n;

    for (n = 0; s[n]; n++);
    return n;
}