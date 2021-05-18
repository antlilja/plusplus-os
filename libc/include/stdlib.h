#pragma once

#include <stddef.h>

typedef struct {
    int quot;
    int rem;
} div_t;

typedef struct {
    long quot;
    long rem;
} ldiv_t;

int atoi(const char* str);
long int atol(const char* str);
long long int atoll(const char* str);
double atof(const char* str);
double strtod(const char* str, char** endptr);
void* bsearch(const void* key, const void* base, size_t nitems, size_t size,
              int (*compar)(const void*, const void*));
int abs(int x);
long int labs(long int x);
div_t div(int numer, int denom);
ldiv_t ldiv(long int numer, long int denom);