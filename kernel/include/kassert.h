#pragma once

#ifdef ENABLE_KERNEL_ASSERTS
#    define __STR(x) #    x
#    define __XSTR(x) __STR(x)
#    include "rendering.h"
#    define KERNEL_ASSERT(CONDITION, STR)                  \
        if (__builtin_expect(!(CONDITION), 0)) {           \
            clear_screen(0);                               \
            g_fg_color = 0xff0000;                         \
            put_string(STR, 0, 0);                         \
            uint64_t __x = put_string(__FILE__ ":", 0, 1); \
            __x += put_string(__func__, __x, 1);           \
            put_string(":L"__XSTR(__LINE__), __x, 1);      \
            while (1)                                      \
                ;                                          \
        }
#else
#    define KERNEL_ASSERT(CONDITION, STR)
#endif
