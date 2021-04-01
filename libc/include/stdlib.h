#pragma once

#define size_t unsigned

double atof(const char *str){
    //TODO
}
int atoi(const char *str){
    //TODO
}
long int atol(const char *str){
    //TODO
}
double strtod(const char *str, char **endptr){
    //TODO
}
long int strtol(const char *str, char **endptr, int base){
    //TODO
}
unsigned long int strtoul(const char *str, char **endptr, int base){
    //TODO
}
void *calloc(size_t nitems, size_t size){
    //TODO
}
void free(void *ptr){
    //TODO
}
void *malloc(size_t size){
    //TODO
}
void *realloc(void *ptr, size_t size){
    //TODO
}
void abort(void){
    //TODO
}
int atexit(void (*func)(void)){
    //TODO
}
void exit(int status){
    //TODO
}
char *getenv(const char *name){
    //TODO
}
int system(const char *string){
    //TODO
}
void *bsearch(const void *key, const void *base, size_t nitems, size_t size, int (*compar)(const void *, const void *)){
    //TODO
}
void qsort(void *base, size_t nitems, size_t size, int (*compar)(const void *, const void*)){
    //TODO
}
int abs(int x){
    //TODO
}
int div(int numer, int denom){
    //TODO
}
long int labs(long int x){
    //TODO
}
int ldiv(long int numer, long int denom){
    //TODO
}
int rand(void){
    //TODO
}
void srand(unsigned int seed){
    //TODO
}
int mblen(const char *str, size_t n){
    //TODO
}
size_t mbstowcs(int *pwcs, const char *str, size_t n){
    //TODO
}
int mbtowc(int *pwc, const char *str, size_t n){
    //TODO
}
size_t wcstombs(char *str, const wchar_t *pwcs, size_t n){
    //TODO
}
int wctomb(char *str, wchar_t wchar){
    //TODO
}
