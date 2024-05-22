#include <stdlib.h>
#include <stdio.h>

int main(void) {
    void* ptr_2 = malloc(11);
    free(ptr_2);

    return 0;
}
