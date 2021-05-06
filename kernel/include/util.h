#pragma once
#include <stdint.h>
#include <stdbool.h>

// Return maximum value of a and b
#define MAX(a, b)             \
    ({                        \
        __auto_type _a = (a); \
        __auto_type _b = (b); \
        _a > _b ? _a : _b;    \
    })

// Return minimum value of a and b
#define MIN(a, b)             \
    ({                        \
        __auto_type _a = (a); \
        __auto_type _b = (b); \
        _a < _b ? _a : _b;    \
    })

// Rounds n up to nearest multiple of m
uint64_t round_up_to_multiple(uint64_t n, uint64_t m);

// Checks whether or not the value x is within the range [lower, lower + size)
bool range_contains(uint64_t x, uint64_t lower, uint64_t size);

// Checks whether or not the value x is within the range [lower, upper)
bool bound_contains(uint64_t x, uint64_t lower, uint64_t upper);
