#include <stdlib.h>

int main(void) {
    void* ptr = malloc(10);
    void* ptr_2 = malloc(11);
    free(ptr);

    return 0;
}
