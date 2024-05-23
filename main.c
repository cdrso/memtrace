#include <stdlib.h>
#include <stdio.h>

int main(void) {
    void* ptr = malloc(69);
    void* ptr_2 = malloc(69);
    free(ptr);
    return 0;
}
