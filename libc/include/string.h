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
}
void *memcpy(void *dest, const void *src, size_t n){
    //TODO
}
void *memmove(void *dest, const void *src, size_t n){
    //TODO
}
void *memset(void *str, int c, size_t n){
    //TODO
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
    //TODO
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
