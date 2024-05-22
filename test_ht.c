#include <stdio.h>
#include <stdlib.h>
#include <stdbool.h>
#include <stdint.h>
#include <pthread.h>
#include <semaphore.h>
#include "hashmap.h" // Include your hashmap interface

// Number of threads for testing
#define NUM_THREADS 4

// Number of keys per thread
#define KEYS_PER_THREAD 100

// Maximum key value for testing
#define MAX_KEY 1000

// Function for threads to insert and delete their own keys
void* insert_delete_entries(void* arg) {
    hashTable* ht = (hashTable*)arg;
    size_t thread_id = (size_t)pthread_self(); // Unique thread identifier
    size_t start_key = thread_id * KEYS_PER_THREAD + 1; // Starting key for this thread
    for (size_t i = start_key; i < start_key + KEYS_PER_THREAD; i++) {
        allocInfo value;
        value.block_size = i % 100 + 1; // Block size varies from 1 to 100
        ht_insert(ht, i, value); // Insert the key
        ht_delete(ht, i); // Delete the key immediately
    }
    pthread_exit(NULL);
}

int main() {
    // Create the hash table
    hashTable* ht = ht_create();

    // Create threads for insertion and deletion
    pthread_t threads[NUM_THREADS];

    // Create threads for insertion and deletion
    for (int i = 0; i < NUM_THREADS; i++) {
        pthread_create(&threads[i], NULL, insert_delete_entries, ht);
    }

    // Join threads
    for (int i = 0; i < NUM_THREADS; i++) {
        pthread_join(threads[i], NULL);
    }

    // Print the hashtable contents for debugging
    ht_print_debug(ht);

    // Destroy the hash table and cleanup
    ht_destroy(ht);

    return 0;
}
