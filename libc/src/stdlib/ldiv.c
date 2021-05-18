#include <stdlib.h>

ldiv_t ldiv(long int numer, long int denom) {
    ldiv_t num;
    num.quot = numer / denom;
    num.rem = numer % denom;
    return num;
}
