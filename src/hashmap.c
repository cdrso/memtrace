#define _GNU_SOURCE
#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <pthread.h>
#include <semaphore.h>
#include "hashmap.h"
#include "shmwrap.h"

/**
 * The primes array is used both for both capacity values (odd indexes) and
 * double hashing prime values (even indexes)
 */
const static uint32_t primes[] = {
    101,    103,     199,     211,     419,     421,     829,      839,      1637,
    1657,   3301,    3307,    6559,    6607,    13217,   13219,    26437,    26449,
    53003,  53017,   109987,  110017,  220009,  220013,  440009,   440023,   880001,
    880007, 1900009, 1900037, 3800021, 3800051, 7600013, 7600031,  15200011, 15200021
};

const static hashTableEntry clear_entry = {
    .key = 0,
    .value = {
        .block_size = 0,
        .stack_trace = {0, 0, 0, 0, 0, 0, 0, 0, 0, 0}
    }
};


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

#define INITIAL_CAPACITY_INDEX 1
#define LAST_CAPACITY_INDEX 35

#define RESIZE_UP_FACTOR 0.7
#define RESIZE_DOWN_FACTOR 0.2

#define LOAD_FACTOR(ht) \
    (float)ht->length / HT_GET_CAPACITY(ht)

/**
 * The shared memory id for the hashtable is stored as an evoiroment variable
 * when the parent process (memtrace) forks to run the child (executable to analyze)
 * the envoiroment variable is cloned to the new process
 */
#define GET_HT_SHMID atoi(getenv("HT_SHMID"))

#define HT_SHM_KEY_GEN \
    ftok("/tmp", 'A')
#define HT_ENTRIES_SHM_KEY_GEN \
    ftok("/tmp", 'B')
#define HT_MUTEX_SHM_KEY_GEN \
    ftok("/tmp", 'C')


#define FNV_OFFSET_BASIS 0xCBF29CE484222325ULL
#define FNV_PRIME 0x100000001B3ULL

#define DOUBLE_HASH(address, prime, i, capacity) \
    (hash_fnv1(address) + i * (prime - (address % prime))) % capacity


/**
 * ht_load_context locks the ht mutex,
 * it's the callers responsibility to unlock it
 */
static bool ht_load_context(hashTable* ht);
static bool ht_size_up(hashTable* ht);
static bool ht_size_down(hashTable* ht);
static size_t hash_fnv1(size_t address);

/**
 * functions that handle dynamic memory but are not intercepted (the intercept does nothing),
 * defined here as weak and redefined by the shared lib
 */
__attribute__((weak)) void* no_intercept_calloc(size_t num_elements, size_t element_size) { return NULL; }
__attribute__((weak)) void  no_intercept_free(void* ptr) { }


hashTable* ht_create() {
    const int shmid_ht = shmalloc(HT_SHM_KEY_GEN, sizeof(hashTable));
    if (shmid_ht < 1) {
        return NULL;
    }
    hashTable* ht = shmload(shmid_ht);

    // Set the ht shmid as an envoiroment variable to pass to child process
    char shmid_ht_str[256];
    sprintf(shmid_ht_str, "%d", shmid_ht);
    setenv("HT_SHMID", shmid_ht_str, 1);

    ht->length = 0;
    ht->capacity_index = INITIAL_CAPACITY_INDEX;

    const int shmid_ht_mutex = shmalloc(HT_MUTEX_SHM_KEY_GEN, sizeof(pthread_mutex_t));
    if (shmid_ht_mutex < 1) {
        return NULL;
    }
    pthread_mutex_t* ht_mutex = shmload(shmid_ht_mutex);

    if (pthread_mutex_init(ht_mutex, NULL)!= 0) {
        return NULL;
    }

    const int shmid_ht_entries = shmalloc(HT_ENTRIES_SHM_KEY_GEN, sizeof(hashTableEntry) * HT_GET_CAPACITY(ht));
    if (shmid_ht_entries < 1) {
        return NULL;
    }
    hashTableEntry* ht_entries = shmload(shmid_ht_entries);

    ht->entries_shmid = shmid_ht_entries;
    ht->entries = ht_entries;

    for (int i = 0; i < HT_GET_CAPACITY(ht); i++) {
        ht->entries[i] = clear_entry;
    }

    ht->mutex_shmid = shmid_ht_mutex;
    ht->mutex = ht_mutex;

    return ht;
}

void ht_destroy(hashTable* ht) {
    if (!ht) { return; }

    if (pthread_mutex_destroy(ht->mutex)!= 0) {
        perror("Mutex destruction failed");
        exit(EXIT_FAILURE);
        // is this failure critical?
    }
    shmfree(ht->mutex, ht->mutex_shmid);

    shmfree(ht->entries, ht->entries_shmid);

    shmfree(ht, GET_HT_SHMID);
}

bool ht_insert(hashTable* ht, const size_t key, const allocInfo value) {
    if (!ht) { return false; }

    ht_load_context(ht);

    int i = 0;
    size_t index;
    do {
        index = DOUBLE_HASH(key, HT_GET_HASH_PRIME(ht), i, HT_GET_CAPACITY(ht));
        i++;
    } while (ht->entries[index].key && ht->entries[index].key != key);

    hashTableEntry entry = {
        .key = key,
        .value = value
    };

    if (ht->entries[index].key != key) { ht->length++; }
    ht->entries[index] = entry;

    if (LOAD_FACTOR(ht) > RESIZE_UP_FACTOR && (ht->capacity_index < LAST_CAPACITY_INDEX)) {
        if (!ht_size_up(ht)) {
            return false;
        }
    }

    pthread_mutex_unlock(ht->mutex);

    return true;
}

bool ht_delete(hashTable* ht, const size_t key) {
    if (!ht) { return false; }

    ht_load_context(ht);

    int i = 0;
    size_t index;
    size_t start_index = DOUBLE_HASH(key, HT_GET_HASH_PRIME(ht), 0, HT_GET_CAPACITY(ht));
    size_t found_key = -1;
    do {
        index = DOUBLE_HASH(key, HT_GET_HASH_PRIME(ht), i, HT_GET_CAPACITY(ht));
        if (ht->entries[index].key) {
            found_key = ht->entries[index].key;
        }
        if ((i > 0) && (index == start_index)) {
            pthread_mutex_unlock(ht->mutex);
            return false;
        } i++;
    } while (found_key != key);

    ht->entries[index] = clear_entry;
    ht->length--;

    if (LOAD_FACTOR(ht) < RESIZE_DOWN_FACTOR && (ht->capacity_index > INITIAL_CAPACITY_INDEX)) {
        if (!ht_size_down(ht)) {
            return false;
        }
    }

    pthread_mutex_unlock(ht->mutex);

    return true;
}

const allocInfo* ht_get(hashTable* ht, const size_t key) {
    if (!ht) { return NULL; }

    ht_load_context(ht);

    int i = 0;
    size_t index;
    size_t start_index = DOUBLE_HASH(key, HT_GET_HASH_PRIME(ht), 0, HT_GET_CAPACITY(ht));
    size_t found_key = -1;
    do {
        index = DOUBLE_HASH(key, HT_GET_HASH_PRIME(ht), i, HT_GET_CAPACITY(ht));
        if (ht->entries[index].key) {
            found_key = ht->entries[index].key;
        }
        if ((i > 0) && (index == start_index)) {
            pthread_mutex_unlock(ht->mutex);
            return NULL;
        } i++;
    } while (found_key != key);

    const allocInfo* ret = &ht->entries[index].value;

    pthread_mutex_unlock(ht->mutex);

    return ret;
}

void ht_print_debug(hashTable* ht) {
    if (!ht) {
        printf("Hash table is NULL\n");
        return;
    }
    ht_load_context(ht);

    uint16_t unallocated_blocks_cnt = 0;
    uint16_t unallocated_blocks_bytes = 0;

    for (int i = 0; i < HT_GET_CAPACITY(ht); i++) {
        hashTableEntry entry = ht->entries[i];
        if (entry.key) {
            unallocated_blocks_cnt++;
            unallocated_blocks_bytes += entry.value.block_size;
        }
    }

    if (unallocated_blocks_bytes) {
        printf("%d bytes not freed in %d blocks", unallocated_blocks_bytes, unallocated_blocks_cnt);
    } else {
        printf("No memory leaks");
    }

    pthread_mutex_unlock(ht->mutex);
}


/** Private functions **/
static size_t hash_fnv1(size_t address) {
    uint8_t bytes[8];
    memcpy(bytes, &address, sizeof(address));

    size_t hash = FNV_OFFSET_BASIS;
    for (int i = 0; i < 8; i++) {
        hash *= FNV_PRIME;
        hash ^= bytes[i];
    }

    return hash;
}


static bool ht_load_context(hashTable* ht) {
    pthread_mutex_t* context_mutex = shmload(ht->mutex_shmid);
    if (!context_mutex) {
        return false;
    }
    pthread_mutex_lock(context_mutex);

    hashTableEntry* context_entries = shmload(ht->entries_shmid);
    if (!context_entries) {
        return false;
    }

    ht->entries = context_entries;
    ht->mutex = context_mutex;

    return true;
}


static bool ht_size_up(hashTable* ht) {
    uint32_t current_capacity = HT_GET_CAPACITY(ht);
    uint32_t new_capacity =  HT_GET_NEXT_CAPACITY(ht);

    hashTableEntry* tmp = no_intercept_calloc(current_capacity, sizeof(hashTableEntry));
    memcpy(tmp, ht->entries, current_capacity * sizeof(hashTableEntry));

    shmfree(ht->entries, ht->entries_shmid);

    int shmid_ht_realloc_entries = shmalloc(HT_ENTRIES_SHM_KEY_GEN, sizeof(hashTableEntry) * new_capacity);
    if (shmid_ht_realloc_entries < 1) {
        return NULL;
    }
    hashTableEntry* ht_realloc_entries = shmload(shmid_ht_realloc_entries);

    ht->entries_shmid = shmid_ht_realloc_entries;
    ht->entries = ht_realloc_entries;

    for (int i = 0; i < new_capacity; i++) {
        ht->entries[i] = clear_entry;
    }

    uint32_t new_hash_prime = HT_GET_NEXT_HASH_PRIME(ht);
    for (int i = 0; i < current_capacity; i++) {
        hashTableEntry entry = tmp[i];
        if (entry.key) {
            int j = 0;
            size_t new_index;
            do {
                new_index = DOUBLE_HASH(entry.key, new_hash_prime, j, new_capacity);
                j++;
            } while (ht->entries[new_index].key);

            ht->entries[new_index] = entry;
        }
    }
    no_intercept_free(tmp);

    ht->capacity_index += 2;

    return true;
};


static bool ht_size_down(hashTable* ht) {
    uint32_t current_capacity = HT_GET_CAPACITY(ht);
    uint32_t new_capacity =  HT_GET_PREV_CAPACITY(ht);

    hashTableEntry* tmp = no_intercept_calloc(current_capacity, sizeof(hashTableEntry));
    memcpy(tmp, ht->entries, current_capacity * sizeof(hashTableEntry));

    shmfree(ht->entries, ht->entries_shmid);

    int shmid_ht_realloc_entries = shmalloc(HT_ENTRIES_SHM_KEY_GEN, sizeof(hashTableEntry) * new_capacity);
    if (shmid_ht_realloc_entries < 1) {
        return NULL;
    }
    hashTableEntry* ht_realloc_entries = shmload(shmid_ht_realloc_entries);
    if (!ht_realloc_entries) {
        return NULL;
    }

    ht->entries_shmid = shmid_ht_realloc_entries;
    ht->entries = ht_realloc_entries;

    for (int i = 0; i < new_capacity; i++) {
        ht->entries[i] = clear_entry;
    }

    uint32_t new_hash_prime = HT_GET_PREV_HASH_PRIME(ht);
    for (int i = 0; i < current_capacity; i++) {
        hashTableEntry entry = tmp[i];
        if (entry.key) {
            int j = 0;
            size_t new_index;
            do {
                new_index = DOUBLE_HASH(entry.key, new_hash_prime, j, new_capacity);
                j++;
            } while (ht->entries[new_index].key);

            ht->entries[new_index] = entry;
        }
    }
    no_intercept_free(tmp);

    ht->capacity_index -= 2;

    return true;
};

