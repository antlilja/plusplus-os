#include "util.h"

uint64_t round_up_to_multiple(uint64_t n, uint64_t m) { return ((n + m - 1) / m) * m; }

bool range_contains(uint64_t x, uint64_t lower, uint64_t size) {
    return bound_contains(x, lower, lower + size);
}

bool bound_contains(uint64_t x, uint64_t lower, uint64_t upper) { return x >= lower && x < upper; }
