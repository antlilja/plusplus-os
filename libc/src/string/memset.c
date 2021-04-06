#include <string.h>

void *memset(void *str, int c, size_t n){
    const char data = (char)c;
    char *to = (char*)str;
    for(size_t i = 0; i < n; ++i) to[i]=data;
    return str;
}
