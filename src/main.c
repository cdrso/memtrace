#include <stdio.h>
#include <stdlib.h>
#include <pthread.h>

#define NUM_ALLOCATIONS 10000
#define MAX_ALLOCATION_SIZE 1024

int main(void) {
    void* arr[NUM_ALLOCATIONS];

    for (int i = 0; i < NUM_ALLOCATIONS; i++) {
        size_t size = (rand() % MAX_ALLOCATION_SIZE) + 1;
        arr[i] = malloc(size);
        if (arr[i] == NULL) {
            fprintf(stderr, "Failed to allocate memory\n");
            return -1;
        }
    }
    for (int i = 0; i < NUM_ALLOCATIONS; i++) {
        free(arr[i]);
    }

    return 0;
}
