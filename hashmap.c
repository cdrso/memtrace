#include <pthread.h>
#define _GNU_SOURCE
#include "hashmap.h"
#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <strings.h>
#include <sys/mman.h>
#include <sys/ipc.h>
#include <sys/shm.h>
#include <fcntl.h>
#include <semaphore.h>

//debug
#include <stdio.h>

/**
 * The primes array is used both for both capacity values (odd indexes) and
 * double hashing prime values (even indexes & 0)
 */
const static uint32_t primes[] = {
    101,    103,     199,     211,     419,     421,     829,      839,      1637,
    1657,   3301,    3307,    6559,    6607,    13217,   13219,    26437,    26449,
    53003,  53017,   109987,  110017,  220009,  220013,  440009,   440023,   880001,
    880007, 1900009, 1900037, 3800021, 3800051, 7600013, 7600031,  15200011, 15200021
};


#define INITIAL_CAPACITY_INDEX 1
#define LAST_CAPACITY_INDEX 35

#define RESIZE_UP_FACTOR 0.7
#define RESIZE_DOWN_FACTOR 0.2


#define HT_GET_CAPACITY(ht) \
    primes[ht->capacity_index]
#define HT_GET_NEXT_CAPACITY(ht) \
    primes[ht->capacity_index+2]
#define HT_GET_PREV_CAPACITY(ht) \
    primes[ht->capacity_index-2]
#define HT_GET_HASH_PRIME(ht) \
    primes[ht->capacity_index-1]
#define HT_GET_NEXT_HASH_PRIME(ht) \
    primes[ht->capacity_index+2-1]
#define HT_GET_PREV_HASH_PRIME(ht) \
    primes[ht->capacity_index-2-1]


__attribute__((weak)) void* no_intercept_calloc(size_t num_elements, size_t element_size) { return NULL; }
__attribute__((weak)) void* no_intercept_malloc(size_t size) { return NULL; }
__attribute__((weak)) void  no_intercept_free(void* ptr) { return; }


bool ht_size_up(hashTable* ht);
bool ht_size_down(hashTable* ht);
size_t hash_fnv1(size_t address);


#define FNV_OFFSET_BASIS 0xCBF29CE484222325ULL
#define FNV_PRIME 0x100000001B3ULL


size_t hash_fnv1(size_t address) {
    uint8_t bytes[8];
    memcpy(bytes, &address, sizeof(address));

    size_t hash = FNV_OFFSET_BASIS;
    for (int i = 0; i < 8; i++) {
        hash *= FNV_PRIME;
        hash ^= bytes[i];
    }

    return hash;
}


#define DOUBLE_HASH(address, prime, i, capacity) \
    (hash_fnv1(address) + i * (prime - (address % prime))) % capacity


bool ht_create(hashTable** ht) {
    *ht = (hashTable*)malloc(sizeof(hashTable));
    if (!*ht) { return NULL; }

    (*ht)->length = 0;
    (*ht)->capacity_index = INITIAL_CAPACITY_INDEX;

    hashTableEntry** ht_entries = (hashTableEntry**)calloc(HT_GET_CAPACITY((*ht)), sizeof(hashTableEntry*));
    if (!ht_entries) { return NULL; }
    (*ht)->entries = ht_entries;

    pthread_mutex_t* ht_mutex = (pthread_mutex_t*)malloc(sizeof(pthread_mutex_t));
    if (!ht_mutex) { return NULL; }

    if (pthread_mutex_init(ht_mutex, NULL)!= 0) {
        perror("Mutex initialization failed");
        exit(EXIT_FAILURE);
    }

    (*ht)->mutex = ht_mutex;

    return true;
}


void ht_destroy(hashTable* ht) {
    if (!ht) { return; }

    for (int i = 0; i < HT_GET_CAPACITY(ht) ; i++) {
        no_intercept_free(ht->entries[i]);
    }
    no_intercept_free(ht->entries);

    if (pthread_mutex_destroy(ht->mutex)!= 0) {
        perror("Mutex destruction failed");
        exit(EXIT_FAILURE);
    }
    no_intercept_free(ht->mutex);

    no_intercept_free(ht);
}


bool ht_insert(hashTable* ht, const size_t key, const allocInfo value) {
    if (!ht) {
        printf("ht input is NULL\n");
        return false;
    }

    pthread_mutex_lock(ht->mutex);

    int i = 0;
    size_t index;
    do {
        index = DOUBLE_HASH(key, HT_GET_HASH_PRIME(ht), i, HT_GET_CAPACITY(ht));
        i++;
    } while (ht->entries[index] && ht->entries[index]->key != key);

    hashTableEntry* entry = (hashTableEntry*)no_intercept_malloc(sizeof(hashTableEntry));

    entry->key = key;
    entry->value = value;


    if (ht->entries[index]->key != key) {
    ht->length++;
    }

    ht->entries[index] = entry;

    if (((float)ht->length / HT_GET_CAPACITY(ht)) >= RESIZE_UP_FACTOR && (ht->capacity_index < LAST_CAPACITY_INDEX)) {
        ht_size_up(ht);
    }

    pthread_mutex_unlock(ht->mutex);

    return true;
}


bool ht_delete(hashTable* ht, const size_t key) {
    if (!ht) {
        printf("ht input is NULL\n");
        return false;
    }

    pthread_mutex_lock(ht->mutex);

    int i = 0;
    size_t index;
    size_t start_index = DOUBLE_HASH(key, HT_GET_HASH_PRIME(ht), 0, HT_GET_CAPACITY(ht));
    size_t found_key = -1;
    do {
        index = DOUBLE_HASH(key, HT_GET_HASH_PRIME(ht), i, HT_GET_CAPACITY(ht));
        if (ht->entries[index]) {
            found_key = ht->entries[index]->key;
        }
        if ((i > 0) && (index == start_index)) {
            pthread_mutex_unlock(ht->mutex);
            return false;
        }
        i++;
    } while (found_key != key);

    ht->entries[index] = NULL;

    ht->length--;

    if (((float)ht->length / HT_GET_CAPACITY(ht)) <= RESIZE_DOWN_FACTOR && (ht->capacity_index > INITIAL_CAPACITY_INDEX)) {
        ht_size_down(ht);
    }

    pthread_mutex_unlock(ht->mutex);

    return true;
}


allocInfo* ht_get(hashTable* ht, const size_t key) {
    if (!ht) { return NULL; }

    pthread_mutex_lock(ht->mutex);

    int i = 0;
    size_t index;
    size_t start_index = DOUBLE_HASH(key, HT_GET_HASH_PRIME(ht), 0, HT_GET_CAPACITY(ht));
    size_t found_key = -1;
    do {
        index = DOUBLE_HASH(key, HT_GET_HASH_PRIME(ht), i, HT_GET_CAPACITY(ht));
        if (ht->entries[index]) {
            found_key = ht->entries[index]->key;
        }
        if ((i > 0) && (index == start_index)) {
            pthread_mutex_unlock(ht->mutex);
            return NULL;
        }
        i++;
    } while (found_key != key);

    allocInfo* ret = &ht->entries[index]->value;

    pthread_mutex_unlock(ht->mutex);

    return ret;
}


bool ht_size_up(hashTable* ht) {
    int32_t current_capacity = HT_GET_CAPACITY(ht);
    int32_t new_capacity =  HT_GET_NEXT_CAPACITY(ht);

    hashTableEntry** tmp = no_intercept_calloc(current_capacity, sizeof(hashTableEntry*));
    memcpy(tmp, ht->entries, current_capacity * sizeof(hashTableEntry*));

    no_intercept_free(ht->entries);

    hashTableEntry** new_entries = (hashTableEntry**)no_intercept_calloc(new_capacity, sizeof(hashTableEntry*));

    ht->entries = new_entries;

    int32_t new_hash_prime = HT_GET_NEXT_HASH_PRIME(ht);
    for (int i = 0; i < current_capacity; i++) {
        hashTableEntry* entry = tmp[i];
        if (entry) {
            int j = 0;
            size_t new_index;
            do {
                new_index = DOUBLE_HASH(entry->key, new_hash_prime, j, new_capacity);
                j++;
            } while (ht->entries[new_index]);

            ht->entries[new_index] = entry;
        }
    }
    no_intercept_free(tmp);

    ht->capacity_index += 2;

    return true;
};


bool ht_size_down(hashTable* ht) {
    int32_t current_capacity = HT_GET_CAPACITY(ht);
    int32_t new_capacity =  HT_GET_PREV_CAPACITY(ht);

    hashTableEntry** tmp = no_intercept_calloc(current_capacity, sizeof(hashTableEntry*));
    memcpy(tmp, ht->entries, current_capacity * sizeof(hashTableEntry*));

    no_intercept_free(ht->entries);

    hashTableEntry** new_entries = (hashTableEntry**)no_intercept_calloc(new_capacity, sizeof(hashTableEntry*));

    ht->entries = new_entries;

    int32_t new_hash_prime = HT_GET_PREV_HASH_PRIME(ht);
    for (int i = 0; i < current_capacity; i++) {
        hashTableEntry* entry = tmp[i];
        if (entry) {
            int j = 0;
            size_t new_index;
            do {
                new_index = DOUBLE_HASH(entry->key, new_hash_prime, j, new_capacity);
                j++;
            } while (ht->entries[new_index]);

            ht->entries[new_index] = entry;
        }
    }
    no_intercept_free(tmp);

    ht->capacity_index -= 2;

    return true;
};


void ht_print_debug(hashTable* ht) {
    if (!ht) {
        printf("Hash table is NULL\n");
        return;
    }

    pthread_mutex_lock(ht->mutex);
    //ht_load_context(ht);

    printf("Hash table capacity: %d\n", HT_GET_CAPACITY(ht));
    printf("Hash table length: %d\n", ht->length);

    printf("Hash table entries:\n");
    for (int i = 0; i < HT_GET_CAPACITY(ht); i++) {
        hashTableEntry* entry = ht->entries[i];
        if (entry) {
            printf("[%d] Key: 0x%-10zx -  Block Size: %-10u\n", i, entry->key, entry->value.block_size);
            // Add code to print stack trace if needed
        } else {
            printf("[%d] (Empty)\n", i);
        }
    }

    pthread_mutex_unlock(ht->mutex);
}
