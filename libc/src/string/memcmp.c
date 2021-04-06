#include <string.h>

int memcmp(const void *str1, const void *str2, size_t n){
    const char *c1 = (const char*)str1;
    const char *c2 = (const char*)str2;
    for(size_t i = 0; i < n; ++i) {
        if(c1[i] != c2[i]) return c1[i]-c2[i];
    }
    return 0;
}
