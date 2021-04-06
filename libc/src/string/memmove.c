#include <string.h>

void* memmove(void* dest, const void* src, size_t n) {
    const char* from = (const char*)src;
    char* to = (char*)dest;
    if (dest < src) {
        for (size_t i = 0; i < n; ++i) to[i] = from[i];
    }
    else {
        for (size_t i = 1; i <= n; ++i) to[n - i] = from[n - i];
    }
    return dest;
}
