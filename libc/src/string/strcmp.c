#include <string.h>

int strcmp(const char *str1, const char *str2){
    for(size_t i = 0; str1[i] != '\0' && str2[i] != '\0'; ++i) {
        if(str1[i] != str2[i]) return (int)str1[i] - (int)str2[i];
    }
    return 0;
}
