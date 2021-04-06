#include <stdlib.h>

int atoi(const char* str) {
    int res = 0, sign;

    while (*str == ' ') str++;

    if (*str == '-') {
        sign = 1;
        str++;
    }
    else {
        sign = 0;
        if (*str == '+') str++;
    }
    for (; *str - '0' > 0 && *str - '0' < 10; str++) {
        res = (*str - '0') + (10 * res);
    }

    if (sign) return (-1) * res;

    return res;
}
