#pragma once
#include <stdint.h>
#include <stdbool.h>

// Rounds n up to nearest multiple of m
uint64_t round_up_to_multiple(uint64_t n, uint64_t m);

// Checks whether or not the value x is within the range [lower, lower + size)
bool range_contains(uint64_t x, uint64_t lower, uint64_t size);
