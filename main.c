#include <stdio.h>
#include <stdlib.h>
#include <pthread.h>

#define NUM_THREADS 4
#define NUM_ALLOCATIONS 50
#define MAX_ALLOCATION_SIZE 1024

void* thread_function(void* arg) {
    for (int i = 0; i < NUM_ALLOCATIONS; ++i) {
        size_t size = (rand() % MAX_ALLOCATION_SIZE) + 1;
        void* ptr = malloc(size);
        if (ptr == NULL) {
            fprintf(stderr, "Failed to allocate memory\n");
            pthread_exit(NULL);
        }
        free(ptr);
    }
    pthread_exit(NULL);
}

int main() {
    pthread_t threads[NUM_THREADS];

    srand((unsigned int)time(NULL)); // Seed for random numbers

    // Create threads
    for (int i = 0; i < NUM_THREADS; ++i) {
        if (pthread_create(&threads[i], NULL, thread_function, NULL) != 0) {
            fprintf(stderr, "Error creating thread\n");
            return 1;
        }
    }

    // Wait for threads to finish
    for (int i = 0; i < NUM_THREADS; ++i) {
        if (pthread_join(threads[i], NULL) != 0) {
            fprintf(stderr, "Error joining thread\n");
            return 1;
        }
    }

    return 0;
}

