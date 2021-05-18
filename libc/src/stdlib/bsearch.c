#include <stdlib.h>

void* bsearch(const void* key, const void* base, size_t nitems, size_t size,
              int (*compar)(const void*, const void*)) {

    const char* middle = base;
    int res;

    while (nitems != 0) {
        middle += (nitems / 2) * size;
        res = (*compar)(key, middle);

        if (res > 0) {
            nitems = (nitems / 2) - ((nitems & 1) == 0);
            middle += size;
            base = middle;
        }

        else if (res < 0) {
            nitems /= 2;
            middle = base;
        }

        else {
            return (char*)middle;
        }
    }
    return NULL;
}