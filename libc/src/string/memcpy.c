#include <string.h>

void* memcpy(void* dest, const void* src, size_t n) {
    const char* from = (const char*)src;
    char* to = (char*)dest;
    for (size_t i = 0; i < n; ++i) to[i] = from[i];
    return dest;
}
