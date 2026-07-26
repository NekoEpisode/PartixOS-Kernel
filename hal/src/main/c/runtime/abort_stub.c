void abort(void) {
    while (1) {}
}

void __stack_chk_fail(void) {
    while (1) {}
}

void *malloc(unsigned long size) {
    (void)size;
    return (void*)0;
}

void free(void *ptr) {
    (void)ptr;
}

void *__memcpy_chk(void *dest, const void *src, unsigned long n, unsigned long destlen) {
    (void)destlen;
    unsigned char *d = (unsigned char *)dest;
    const unsigned char *s = (const unsigned char *)src;
    for (unsigned long i = 0; i < n; i++) d[i] = s[i];
    return dest;
}
