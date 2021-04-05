#pragma once

#ifdef ENABLE_KERNEL_ASSERTS
#    include "rendering.h"
#    define KERNEL_ASSERT(CONDITION, STR) \
        if (!(CONDITION)) {               \
            [[unlikely]] clear_screen(0); \
            g_fg_color = 0xff0000;        \
            put_string(STR, 0, 0);        \
            while (1)                     \
                ;                         \
        }

#else
#    define KERNEL_ASSERT(CONDITION, STR)
#endif
