#include <stdlib.h>

long int atol(const char* str) {
    int sign;
    long int res = 0;

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
