#include <string.h>

void *memmove(void *dest, const void *src, size_t n){
    const char *from = (char*)src;
    char *to = (char*)dest;
    char data[n];
    for(size_t i = 0; i < n; ++i) data[i] = from[i];
    for(size_t i = 0; i < n; ++i) to[i]   = data[i];
    return dest;
}
