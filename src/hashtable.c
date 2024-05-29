/*
 * Copyright (C) 2024 Alejandro Cadarso
 *
 * This file is part of Memtrace.
 *
 * Memtrace is free software: you can redistribute it and/or modify
 * it under the terms of the GNU General Public License as published by
 * the Free Software Foundation, either version 3 of the License, or
 * any later version.
 *
 * This file is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU General Public License for more details.
 *
 * You should have received a copy of the GNU General Public License
 * along with Memtrace.  If not, see <https://www.gnu.org/licenses/>.
 *
 * File: hashmap.c
 * Author: Alejandro Cadarso
 * Date: 28-05-2024
 *
 * This file provides implementation for managing a hash table with shared
 * memory segments in C. The hash table supports dynamic resizing and
 * double hashing for collision resolution. The implementation includes functions
 * for creating, destroying, inserting, deleting, and retrieving entries, as well as
 * printing debug information.
 *
 */

#define _GNU_SOURCE
#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <pthread.h>
#include <unistd.h>
#include <semaphore.h>
#include "hashtable.h"
#include "shmwrap.h"

/**
 * HashTable data structures.
 * Capacity is not stored direcly, it can be retrieved with the HT_GET_CAPACITY macro.
 * Shared memory ids are stored to be able to get correct pointers in any given virtual
 * address space
 */
typedef struct hashTableEntry {
    size_t key;
    allocInfo value;
} hashTableEntry;

struct hashTable {
    uint32_t capacity_index;
    uint32_t length;
    pid_t context;
    int entries_shmid;
    hashTableEntry* entries;
    int mutex_shmid;
    pthread_mutex_t* mutex;
};

/**
 * The primes array is used both for both hashtable capacity values (odd indexes) and
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
// Next odd index
#define HT_GET_NEXT_CAPACITY(ht) \
    primes[ht->capacity_index+2]
// Prev odd index
#define HT_GET_PREV_CAPACITY(ht) \
    primes[ht->capacity_index-2]

#define HT_GET_HASH_PRIME(ht) \
    primes[ht->capacity_index-1]
// Next even index
#define HT_GET_NEXT_HASH_PRIME(ht) \
    primes[ht->capacity_index+2-1]
// Prev even index
#define HT_GET_PREV_HASH_PRIME(ht) \
    primes[ht->capacity_index-2-1]

// Refer to primes array
#define HT_INITIAL_CAPACITY_INDEX 1
#define HT_LAST_CAPACITY_INDEX 35

#define SIZE_UP_LOAD_FACTOR 0.7
#define SIZE_DOWN_LOAD_FACTOR 0.2

#define HT_LOAD_FACTOR(ht) \
    (float)ht->length / HT_GET_CAPACITY(ht)

/**
 * The shared memory id for the hashtable is stored as an env variable
 * when the parent process (memtrace) forks to run the child (executable to analyze)
 * the env variable is cloned to the new process
 */
#define GET_HT_SHMID atoi(getenv("HT_SHMID"))

// Default keys to obtain shmids
#define HT_SHM_KEY_GEN \
    ftok("/tmp", 'A')
#define HT_ENTRIES_SHM_KEY_GEN \
    ftok("/tmp", 'B')
#define HT_MUTEX_SHM_KEY_GEN \
    ftok("/tmp", 'C')


#define FNV_OFFSET_BASIS 0xCBF29CE484222325ULL
#define FNV_PRIME 0x100000001B3ULL

#define DOUBLE_HASH(address, prime, i, capacity) \
    (_hash_fnv1(address) + i * (prime - (address % prime))) % capacity


/**
 * ht_load_context locks the ht mutex,
 * it's the callers responsibility to unlock it
 */
static void _ht_load_context(hashTable* ht);
static bool _ht_size_up(hashTable* ht);
static bool _ht_size_down(hashTable* ht);
static size_t _hash_fnv1(size_t address);


/************************************************************************************************************
 *                                          PUBLIC FUNCTIONS                                                *
 ***********************************************************************************************************/

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
    ht->capacity_index = HT_INITIAL_CAPACITY_INDEX;

    const int shmid_ht_mutex = shmalloc(HT_MUTEX_SHM_KEY_GEN, sizeof(pthread_mutex_t));
    if (shmid_ht_mutex < 1) {
        shmfree(ht, shmid_ht);
        return NULL;
    }
    pthread_mutex_t* ht_mutex = shmload(shmid_ht_mutex);

    if (pthread_mutex_init(ht_mutex, NULL)!= 0) {
        shmfree(ht, shmid_ht);
        shmfree(ht_mutex, shmid_ht_mutex);
        return NULL;
    }

    const int shmid_ht_entries = shmalloc(HT_ENTRIES_SHM_KEY_GEN, sizeof(hashTableEntry) * HT_GET_CAPACITY(ht));
    if (shmid_ht_entries < 1) {
        pthread_mutex_destroy(ht_mutex);
        shmfree(ht, shmid_ht);
        shmfree(ht->mutex, shmid_ht_mutex);
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

    ht->context = getpid();

    return ht;
}

void ht_destroy(hashTable* ht) {
    if (!ht) { return; }

    if (pthread_mutex_destroy(ht->mutex)!= 0) {
        fputs("Mutex destruction failure\n", stderr);
    }

    if (!shmfree(ht->mutex, ht->mutex_shmid) || !shmfree(ht->entries, ht->entries_shmid) || !shmfree(ht, GET_HT_SHMID)) {
        fputs("HashTable deallocation failure\n", stderr);
    }
}

bool ht_insert(hashTable* ht, const size_t key, const allocInfo value) {
    if (!ht) { return false; }

    _ht_load_context(ht);

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

    if (HT_LOAD_FACTOR(ht) > SIZE_UP_LOAD_FACTOR && (ht->capacity_index < HT_LAST_CAPACITY_INDEX)) {
        if (!_ht_size_up(ht)) {
            pthread_mutex_unlock(ht->mutex);
            return false;
        }
    }

    pthread_mutex_unlock(ht->mutex);

    return true;
}

bool ht_delete(hashTable* ht, const size_t key) {
    if (!ht) { return false; }

    _ht_load_context(ht);

    size_t index;
    size_t start_index = DOUBLE_HASH(key, HT_GET_HASH_PRIME(ht), 0, HT_GET_CAPACITY(ht));
    if (ht->entries[start_index].key != key) {
        int i = 1;
        size_t found_key = -1;
        do {
            index = DOUBLE_HASH(key, HT_GET_HASH_PRIME(ht), i, HT_GET_CAPACITY(ht));
            if (ht->entries[index].key) {
                found_key = ht->entries[index].key;
            }
            if (index == start_index) {
                // Non existing entries do not fail deletion
                pthread_mutex_unlock(ht->mutex);
                return true;
            } i++;
        } while (found_key != key);
    } else {
        index = start_index;
    }

    ht->entries[index] = clear_entry;
    ht->length--;

    if (HT_LOAD_FACTOR(ht) < SIZE_DOWN_LOAD_FACTOR && (ht->capacity_index > HT_INITIAL_CAPACITY_INDEX)) {
        if (!_ht_size_down(ht)) {
            pthread_mutex_unlock(ht->mutex);
            return false;
        }
    }

    pthread_mutex_unlock(ht->mutex);

    return true;
}

const allocInfo* ht_get(hashTable* ht, const size_t key) {
    if (!ht) { return NULL; }

    _ht_load_context(ht);

    size_t index;
    size_t start_index = DOUBLE_HASH(key, HT_GET_HASH_PRIME(ht), 0, HT_GET_CAPACITY(ht));
    if (ht->entries[start_index].key != key) {
        int i = 1;
        size_t found_key = -1;
        do {
            index = DOUBLE_HASH(key, HT_GET_HASH_PRIME(ht), i, HT_GET_CAPACITY(ht));
            if (ht->entries[index].key) {
                found_key = ht->entries[index].key;
            }
            if (index == start_index) {
                pthread_mutex_unlock(ht->mutex);
                return false;
            } i++;
        } while (found_key != key);
    } else {
        index = start_index;
    }

    const allocInfo* ret = &ht->entries[index].value;

    pthread_mutex_unlock(ht->mutex);

    return ret;
}

void ht_print_debug(hashTable* ht, bool s_flag) {
    if (!ht) {
        printf("Hash table is NULL\n");
        return;
    }
    _ht_load_context(ht);

    uint32_t unallocated_blocks_cnt = 0;
    uint32_t unallocated_blocks_bytes = 0;

    for (int i = 0; i < HT_GET_CAPACITY(ht); i++) {
        hashTableEntry entry = ht->entries[i];
        if (entry.key) {
            unallocated_blocks_cnt++;
            unallocated_blocks_bytes += entry.value.block_size;
            if (s_flag) {
                printf("\nLeaked Block Size: %d bytes\n", entry.value.block_size);
                printf("Leaked Block Stack Trace:\n\n");
                for (int i = 1; i < MAX_STRINGS; i++) {
                    if (*entry.value.stack_trace[i] != '\0') {
                        printf("# %s\n", entry.value.stack_trace[i]);
                    }
                }
                printf("\nTo track down the leak run:\n");
                printf("objdump -S <executable> | grep -A 10 -B 10 '<top-base-offset>'\n");
                printf("addr2line -e <executable> <top-base-offset>\n\n");
                printf("--------------------------------------------------------------\n");
            }
        }
    }

    if (unallocated_blocks_bytes) {
        printf("%d bytes not freed in %d blocks\n\n", unallocated_blocks_bytes, unallocated_blocks_cnt);
    } else {
        printf("\nNo memory leaks\n\n");
    }

    pthread_mutex_unlock(ht->mutex);
}


/************************************************************************************************************
 *                                          PRIVATE FUNCTIONS                                               *
 ***********************************************************************************************************/


#ifdef HT_TEST
    // For hashtable testing just use stdlib functions
    void* _no_intercept_calloc(size_t num_elements, size_t element_size) {
        void* ptr = calloc(num_elements, element_size);
        return ptr;
    }
    void  _no_intercept_free(void* ptr) {
        free(ptr);
    }
#else
    /**
     * functions that handle dynamic memory but are not intercepted (the intercept does nothing),
     * defined here as weak and redefined by the shared lib
     */
    __attribute__((weak)) void* _no_intercept_calloc(size_t num_elements, size_t element_size) { return NULL; }
    __attribute__((weak)) void  _no_intercept_free(void* ptr) { }
#endif


static size_t _hash_fnv1(size_t address) {
    uint8_t bytes[8];
    memcpy(bytes, &address, sizeof(address));

    size_t hash = FNV_OFFSET_BASIS;
    for (int i = 0; i < 8; i++) {
        hash *= FNV_PRIME;
        hash ^= bytes[i];
    }

    return hash;
}


static void _ht_load_context(hashTable* ht) {
    /**
     * The mutex is taken before being loaded into the current process,
     * in this context loading means updating the ht->entries and ht->mutex pointers
     * for the current virtual address space
     */

    /**
     * For simplicity let's assume that loading memory from
     * a valid shmid never fails
     */

    pid_t new_context = getpid();
    if (ht->context == new_context) {
        pthread_mutex_lock(ht->mutex);
        return;
    }

    pthread_mutex_t* context_mutex = shmload(ht->mutex_shmid);
    hashTableEntry* context_entries = shmload(ht->entries_shmid);

    pthread_mutex_lock(context_mutex);

    ht->entries = context_entries;
    ht->mutex = context_mutex;

    ht->context = new_context;
}


static bool _ht_size_up(hashTable* ht) {
    uint32_t current_capacity = HT_GET_CAPACITY(ht);
    uint32_t new_capacity =  HT_GET_NEXT_CAPACITY(ht);

    hashTableEntry* tmp = _no_intercept_calloc(current_capacity, sizeof(hashTableEntry));
    memcpy(tmp, ht->entries, current_capacity * sizeof(hashTableEntry));

    if (!shmfree(ht->entries, ht->entries_shmid)) {
        _no_intercept_free(tmp);
        return false;
    }

    int shmid_ht_realloc_entries = shmalloc(HT_ENTRIES_SHM_KEY_GEN, sizeof(hashTableEntry) * new_capacity);
    if (shmid_ht_realloc_entries < 1) {
        _no_intercept_free(tmp);
        return false;
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
    _no_intercept_free(tmp);

    ht->capacity_index += 2;

    return true;
};


static bool _ht_size_down(hashTable* ht) {
    uint32_t current_capacity = HT_GET_CAPACITY(ht);
    uint32_t new_capacity =  HT_GET_PREV_CAPACITY(ht);

    hashTableEntry* tmp = _no_intercept_calloc(current_capacity, sizeof(hashTableEntry));
    memcpy(tmp, ht->entries, current_capacity * sizeof(hashTableEntry));

    if (!shmfree(ht->entries, ht->entries_shmid)) {
        _no_intercept_free(tmp);
        return false;
    }

    int shmid_ht_realloc_entries = shmalloc(HT_ENTRIES_SHM_KEY_GEN, sizeof(hashTableEntry) * new_capacity);
    if (shmid_ht_realloc_entries < 1) {
        _no_intercept_free(tmp);
        return false;
    }
    hashTableEntry* ht_realloc_entries = shmload(shmid_ht_realloc_entries);

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
    _no_intercept_free(tmp);

    ht->capacity_index -= 2;

    return true;
};

