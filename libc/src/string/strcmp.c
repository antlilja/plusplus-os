#include <string.h>

int strcmp(const char *str1, const char *str2){
    char *c1 = (char*)str1;
    char *c2 = (char*)str2;
    for(size_t i = 0; c1[i] != '\0' && c2[i] != '\0'; ++i) {
        if(c1[i] != c2[i]) return c1[i]-c2[i];
    }
    return 0;
}
