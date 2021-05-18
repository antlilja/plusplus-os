#include <stdlib.h>

div_t div(int numer, int denom) {
    div_t num;
    num.quot = numer / denom;
    num.rem = numer % denom;
    return num;
}