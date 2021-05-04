#pragma once
#include <stdint.h>

void* Alloc(uint32_t size);
void* AllocAlign(uint32_t size, uint32_t align);

void UhciInit();