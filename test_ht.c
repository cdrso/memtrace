/*
#include <stdio.h>
#include <stdlib.h>
#include <unistd.h>
#include <sys/ipc.h>
#include <sys/shm.h>
#include <sys/wait.h>
#include "hashmap.h"

#define HASH_TABLE_SIZE sizeof(hashTable)
#define HASH_TABLE_ENTRY_SIZE sizeof(hashTableEntry)
#define HASH_TABLE_INIT_ENTRIES 103

#define N 1000

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <time.h>
#include <assert.h>


int main() {
    hashTable* ht = ht_create();
    if (!ht) {
        printf("create returned NULL");
        exit(1);
    }


    for (size_t i = 1; i <= N; i++) {
        size_t key = i;
        allocInfo value = {
           .block_size = i % 10 + 1,
        };
        ht_insert(ht, key, value);
    }
    ht_print_debug(ht);

    for (size_t i = 1; i <= N; i++) {
        size_t key = i;
        if (!ht_get(ht, key)) {
            printf("EPA\n");
        }
    }

    for (size_t i = 1; i <= N; i++) {
        size_t key = i;
        ht_delete(ht, key);
    }

    for (size_t i = 1; i <= N; i++) {
        size_t key = i;
        if (ht_get(ht, key)) {
            printf("EPA\n");
        }
    }

    ht_print_debug(ht);

    for (size_t i = 0; i < N/2; i++) {
        size_t key = i;
        if (ht_get(ht, key)) {
            printf("KEY: %zu NOT_DELETED\n", key);
        }
    }

    allocInfo first = {
        .block_size = 1
    };
    allocInfo second = {
        .block_size = 2
    };

    ht_insert(ht, 1, first);
    ht_insert(ht, 1, second);

    ht_print_debug(ht);

    return 0;
}
*/

#include <stdio.h>
#include <stdlib.h>
#include <stdbool.h>
#include <pthread.h>
#include <assert.h>
#include "hashmap.h"

#define NUM_THREADS 10
#define NUM_OPS 1000
#define NUM_KEYS 200
#define LARGE_NUM_KEYS 10000

size_t keys[NUM_KEYS];
allocInfo values[NUM_KEYS];
bool keys_inserted[NUM_KEYS];

size_t large_keys[LARGE_NUM_KEYS];
allocInfo large_values[LARGE_NUM_KEYS];

// Function prototypes for thread functions
void* thread_insert(void* arg);
void* thread_delete(void* arg);
void* thread_get(void* arg);

// Helper function to generate a random allocInfo
allocInfo generate_random_allocInfo() {
    allocInfo info;
    info.block_size = rand() % 1024;
    for (int i = 0; i < 10; i++) {
        info.stack_trace[i] = NULL; // Simplified for this example
    }
    return info;
}

// Helper function to generate fixed keys
void generate_fixed_keys() {
    for (int i = 0; i < NUM_KEYS; i++) {
        keys[i] = (size_t)(i + 1); // Ensure keys are > 0
        values[i] = generate_random_allocInfo();
        keys_inserted[i] = false;
    }
}

void generate_large_fixed_keys() {
    for (int i = 0; i < LARGE_NUM_KEYS; i++) {
        large_keys[i] = (size_t)(i + 1); // Ensure keys are > 0
        large_values[i] = generate_random_allocInfo();
    }
}

void test_capacity_doubling(hashTable* ht) {
    generate_large_fixed_keys();

    // Insert more elements than initial capacity
    for (int i = 0; i < LARGE_NUM_KEYS; i++) {
        assert(ht_insert(ht, large_keys[i], large_values[i]) == true);  // Check that insert succeeded
    }

    // Retrieve and check elements
    for (int i = 0; i < LARGE_NUM_KEYS; i++) {
        allocInfo* value = ht_get(ht, large_keys[i]);
        assert(value != NULL);  // Assert that the value should be found
        assert(value->block_size == large_values[i].block_size);  // Validate the retrieved value
    }

    // Delete elements
    for (int i = 0; i < LARGE_NUM_KEYS; i++) {
        assert(ht_delete(ht, large_keys[i]) == true);  // Assert that the deletion succeeded
    }

    // Ensure all elements are deleted
    for (int i = 0; i < LARGE_NUM_KEYS; i++) {
        assert(ht_get(ht, large_keys[i]) == NULL);  // Assert that the value should not be found
    }
}

void test_collisions(hashTable* ht) {
    size_t collision_keys[5] = {1, 1 + ht->capacity_index, 1 + 2 * ht->capacity_index, 1 + 3 * ht->capacity_index, 1 + 4 * ht->capacity_index};
    allocInfo collision_values[5];

    for (int i = 0; i < 5; i++) {
        collision_values[i] = generate_random_allocInfo();
        assert(ht_insert(ht, collision_keys[i], collision_values[i]) == true);  // Check that insert succeeded
    }

    // Retrieve and check elements
    for (int i = 0; i < 5; i++) {
        allocInfo* value = ht_get(ht, collision_keys[i]);
        assert(value != NULL);  // Assert that the value should be found
        assert(value->block_size == collision_values[i].block_size);  // Validate the retrieved value
    }

    // Delete elements
    for (int i = 0; i < 5; i++) {
        assert(ht_delete(ht, collision_keys[i]) == true);  // Assert that the deletion succeeded
    }
}

int main() {
    generate_fixed_keys();

    hashTable* ht = ht_create();
    assert(ht != NULL);  // Check that the hash table was created successfully

    // Insert elements into the hash table (single-threaded phase)
    for (int i = 0; i < NUM_KEYS / 2; i++) {
        assert(ht_insert(ht, keys[i], values[i]) == true);  // Check that insert succeeded
        keys_inserted[i] = true;
    }

    // Retrieve and check elements from the hash table (single-threaded phase)
    for (int i = 0; i < NUM_KEYS / 2; i++) {
        allocInfo* value = ht_get(ht, keys[i]);
        assert(value != NULL);  // Assert that the value should be found
        assert(value->block_size == values[i].block_size);  // Validate the retrieved value
    }

    // Delete elements from the hash table (single-threaded phase)
    for (int i = 0; i < NUM_KEYS / 4; i++) {
        assert(ht_delete(ht, keys[i]) == true);  // Assert that the deletion succeeded
        keys_inserted[i] = false;
    }

    // Test capacity doubling
    test_capacity_doubling(ht);

    // Test handling of collisions
    test_collisions(ht);

    // Test multithreading
    pthread_t threads[NUM_THREADS];

    for (int i = 0; i < NUM_THREADS; i++) {
        if (i % 3 == 0) {
            pthread_create(&threads[i], NULL, thread_insert, ht);
        } else if (i % 3 == 1) {
            pthread_create(&threads[i], NULL, thread_delete, ht);
        } else {
            pthread_create(&threads[i], NULL, thread_get, ht);
        }
    }

    for (int i = 0; i < NUM_THREADS; i++) {
        pthread_join(threads[i], NULL);
    }

    // Print hash table contents for debug purposes
    ht_print_debug(ht);

    // Clean up
    ht_destroy(ht);

    return 0;
}

void* thread_insert(void* arg) {
    hashTable* ht = (hashTable*)arg;
    for (int i = NUM_KEYS / 2; i < NUM_KEYS; i++) {
        if (!keys_inserted[i]) {
            ht_insert(ht, keys[i], values[i]);
            keys_inserted[i] = true;
        }
    }
    return NULL;
}

void* thread_delete(void* arg) {
    hashTable* ht = (hashTable*)arg;
    for (int i = NUM_KEYS / 2; i < NUM_KEYS; i++) {
        if (keys_inserted[i]) {
            ht_delete(ht, keys[i]);
            keys_inserted[i] = false;
        }
    }
    return NULL;
}

void* thread_get(void* arg) {
    hashTable* ht = (hashTable*)arg;
    for (int i = NUM_KEYS / 2; i < NUM_KEYS; i++) {
        if (keys_inserted[i]) {
            allocInfo* value = ht_get(ht, keys[i]);
            if (value != NULL) {
                assert(value->block_size == values[i].block_size);
            }
        }
    }
    return NULL;
}
