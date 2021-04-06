#include <string.h>

void *memchr(const void *str, int c, size_t n) {
    char data = (char)c;
    char *curr = (char*)str;
    for(size_t i = 0; i < n; ++i) {
        if (*(curr + i) == data) return (void*)curr;
    }
    return NULL;
}
