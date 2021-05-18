#include <stdlib.h>

double atof(const char* str) {
    int sign;

    while (*str == ' ') str++;

    if (*str == '-') {
        sign = 1;
        str++;
    }
    else {
        sign = 0;
        if (*str == '+') str++;
    }

    double res = 0, numOfDec = 1;

    int decPt = 0;
    for (; *str != '\0'; str++) {
        if (*str == '.') {
            decPt = 1;
            continue;
        };
        if (*str >= '0' && *str <= '9') {
            if (decPt) numOfDec /= 10.0;
            res = res * 10.0 + (double)(*str - '0');
        };
    };

    if (sign) res = -res;

    return res * numOfDec;
}