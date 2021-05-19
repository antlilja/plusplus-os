#pragma once

#include <stdbool.h>

extern bool g_kbstatus[0x80];

void register_ps2_interrupt();

// Waits for, and returns pressed character
char ps2_getch();
