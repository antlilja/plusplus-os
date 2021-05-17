#pragma once
#include <stdint.h>
#include <stdbool.h>
#include "memory/defs.h"

bool setup_ahci();

bool read_to_buffer(uint8_t port_no, uint64_t start, uint32_t count, VirtualAddress vbuffer);
