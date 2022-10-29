#include <unistd.h>
#define MAX_VAL 100000000

void* smalloc(size_t size) {
    if (size == 0 || size > MAX_VAL) {
        return NULL;
    }
    void* prog_break = sbrk(size);
    if (prog_break == (void*) -1) {
        return NULL;
    }
    return prog_break;
}