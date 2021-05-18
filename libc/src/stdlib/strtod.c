#include <stdlib.h>

double strtod(const char* str, char** endptr) {

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
    while ((*str >= '0' && *str <= '9') || (*str == '.')) {
        if (*str == '.') {
            decPt = 1;
            str++;
            continue;
        };
        if (*str >= '0' && *str <= '9') {
            if (decPt) numOfDec /= 10.0;
            res = res * 10.0 + (double)(*str - '0');
        };
        str++;
    };

    if (endptr != NULL) *endptr = (char*)str;

    if (sign) res = -res;

    return res * numOfDec;
}