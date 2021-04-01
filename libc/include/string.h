#pragma once

#define size_t unsigned

void *memchr(const void *str, int c, size_t n){
    //TODO
}
int memcmp(const void *str1, const void *str2, size_t n){
    char *c1 = (char*)str1;
    char *c2 = (char*)str2;
    for(size_t i = 0; i < n; ++i) {
        if(c1[i] != c2[i]) return c1[i]-c2[i];
    }
    return 0;
}
void *memcpy(void *dest, const void *src, size_t n){
    char *from = (char*)src;
    char *to = (char*)dest;
    for(size_t i = 0; i < n; ++i) to[i] = from[i];
    return dest;
}
void *memmove(void *dest, const void *src, size_t n){
    return memcpy(dest, src, n);
}
void *memset(void *str, int c, size_t n){
    const char data = (char)c;
    char *to = (char*)str;
    for(size_t i = 0; i < n; ++i) to[i]=data;
    return str;
}
char *strcat(char *dest, const char *src){
    //TODO
}
char *strncat(char *dest, const char *src, size_t n){
    //TODO
}
char *strchr(const char *str, int c){
    //TODO
}
int strcmp(const char *str1, const char *str2){
    char *c1 = (char*)str1;
    char *c2 = (char*)str2;
    for(size_t i = 0; c1[i] != '\0' && c2[i] != '\0'; ++i) {
        if(c1[i] != c2[i]) return c1[i]-c2[i];
    }
    return 0;
}
int strncmp(const char *str1, const char *str2, size_t n){
    //TODO
}
int strcoll(const char *str1, const char *str2){
    //TODO
}
char *strcpy(char *dest, const char *src){
    //TODO
}
char *strncpy(char *dest, const char *src, size_t n){
    //TODO
}
size_t strcspn(const char *str1, const char *str2){
    //TODO
}
char *strerror(int errnum){
    //TODO
}
size_t strlen(const char *str){
    //TODO
}
char *strpbrk(const char *str1, const char *str2){
    //TODO
}
char *strrchr(const char *str, int c){
    //TODO
}
size_t strspn(const char *str1, const char *str2){
    //TODO
}
char *strstr(const char *haystack, const char *needle){
    //TODO
}
char *strtok(char *str, const char *delim){
    //TODO
}
size_t strxfrm(char *dest, const char *src, size_t n){
    //TODO
}
