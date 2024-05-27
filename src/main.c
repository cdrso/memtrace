#include <stdio.h>
#include <stdlib.h>
#include <pthread.h>

#define NUM_ALLOCATIONS 1115
#define MAX_ALLOCATION_SIZE 1024
/*
#define NUM_THREADS 2

void* thread_function(void* arg) {
    void* arr[NUM_ALLOCATIONS];
    for (int i = 0; i < NUM_ALLOCATIONS; i++) {
        size_t size = (rand() % MAX_ALLOCATION_SIZE) + 1;
        arr[i] = malloc(size);
        if (arr[i] == NULL) {
            fprintf(stderr, "Failed to allocate memory\n");
            pthread_exit(NULL);
        }
    }
    for (int i = 0; i < NUM_ALLOCATIONS; i++) {
        free(arr[i]);
    }
    pthread_exit(NULL);
}

int main() {
    pthread_t threads[NUM_THREADS];

    srand((unsigned int)time(NULL)); // Seed for random numbers

    // Create threads
    for (int i = 0; i < NUM_THREADS; i++) {
        if (pthread_create(&threads[i], NULL, thread_function, NULL) != 0) {
            fprintf(stderr, "Error creating thread\n");
            return 1;
        }
    }

    // Wait for threads to finish
    for (int i = 0; i < NUM_THREADS; i++) {
        if (pthread_join(threads[i], NULL) != 0) {
            fprintf(stderr, "Error joining thread\n");
            return 1;
        }
    }

    return 0;
}
*/

int main(void) {
    void* arr[NUM_ALLOCATIONS];
    for (int i = 0; i < NUM_ALLOCATIONS; i++) {
        size_t size = (rand() % MAX_ALLOCATION_SIZE) + 1;
        arr[i] = malloc(size);
        if (arr[i] == NULL) {
            fprintf(stderr, "Failed to allocate memory\n");
            pthread_exit(NULL);
        }
    }
    for (int i = 0; i < NUM_ALLOCATIONS-1; i++) {
        free(arr[i]);
    }

}
