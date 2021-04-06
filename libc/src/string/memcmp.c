#include <string.h>

int memcmp(const void *str1, const void *str2, size_t n){
    char *c1 = (char*)str1;
    char *c2 = (char*)str2;
    for(size_t i = 0; i < n; ++i) {
        if(c1[i] != c2[i]) return c1[i]-c2[i];
    }
    return 0;
}
