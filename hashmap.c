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

//debug
#include <stdio.h>

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

hashTableEntry clear_entry = {
    .key = 0,
    .value = {
        .block_size = 0,
        .stack_trace = {0, 0, 0, 0, 0, 0, 0, 0, 0, 0}
    }
};

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

bool ht_create(hashTable* ht, hashTableEntry* entries) {
    if (!ht || !entries) { return false; }

    ht->capacity_index = INITIAL_CAPACITY_INDEX;
    uint32_t capacity = HT_GET_CAPACITY(ht);

    ht->entries = entries;

    for (int i = 0; i < capacity; i++) {
        ht->entries[i] = clear_entry;
    }

    ht->length = 0;

    return true;
}

void ht_destroy(hashTable* ht) {
    if (!ht) { return; }

    shmdt(ht->entries);

    shmdt(ht);
}

bool ht_insert(hashTable* ht, const size_t key, const allocInfo value) {
    if (!ht) {
        printf("ht input is NULL\n");
        return false;
    }

    uint32_t capacity = HT_GET_CAPACITY(ht);
    uint32_t hash_prime = HT_GET_HASH_PRIME(ht);

    int i = 0;
    int index;
    do {
        index = DOUBLE_HASH(key, hash_prime, i, capacity);
        i++;
    } while (ht->entries[index].key);

    hashTableEntry entry = {
        .key = key,
        .value = value
    };
    ht->entries[index] = entry;

    ht->length++;

    if (((float)ht->length / capacity) >= RESIZE_UP_FACTOR && (ht->capacity_index < LAST_CAPACITY_INDEX)) {
        ht_size_up(ht);
    } else { return false; }

    return true;
}

bool ht_delete(hashTable* ht, const size_t key) {
    if (!ht) {
        printf("ht input is NULL\n");
        return false;
    }

    uint32_t capacity = HT_GET_CAPACITY(ht);
    uint32_t hash_prime = HT_GET_HASH_PRIME(ht);

    // Can´t use ht_get because the index is needed to set that entry to NULL

    int i = 0;
    int index;
    int start_index = DOUBLE_HASH(key, hash_prime, 0, capacity);
    int found_key = -1;
    do {
        index = DOUBLE_HASH(key, hash_prime, i, capacity);
        if (ht->entries[index].key) {
            found_key = ht->entries[index].key;
        }
        if ((i > 0) && (index == start_index)) { return false; }
        i++;
    } while (found_key != key);

    ht->entries[index] = clear_entry;

    ht->length--;

    if (((float)ht->length / capacity) <= RESIZE_DOWN_FACTOR && (ht->capacity_index > INITIAL_CAPACITY_INDEX)) {
        ht_size_down(ht);
    }

    return true;
}

allocInfo* ht_get(hashTable* ht, const size_t key) {
    if (!ht) { return NULL; }

    uint32_t capacity = HT_GET_CAPACITY(ht);
    uint32_t hash_prime = HT_GET_HASH_PRIME(ht);

    int i = 0;
    int index;
    int start_index = DOUBLE_HASH(key, hash_prime, 0, capacity);
    int found_key = -1;
    do {
        index = DOUBLE_HASH(key, hash_prime, i, capacity);
        if (ht->entries[index].key) {
            found_key = ht->entries[index].key;
        }
        if ((i > 0) && (index == start_index)) { return NULL; }
        i++;
    } while (found_key != key);

    return &ht->entries[index].value;
}

#define GET_ENTRIES_SHMID atoi(getenv("HT_ENTRIES_SHMID"))
bool ht_size_up(hashTable* ht) {
    int32_t current_capacity = HT_GET_CAPACITY(ht);
    int32_t new_capacity =  HT_GET_NEXT_CAPACITY(ht);

    hashTableEntry* tmp = calloc(current_capacity, sizeof(hashTableEntry));
    memcpy(tmp, ht->entries, current_capacity * sizeof(hashTableEntry));


    /* same parameters as parent process gives same id */
    key_t shkey_realloc_entries = ftok("/tmp", 'B');

    int dgb = GET_ENTRIES_SHMID;
    shmdt(ht->entries);
    int dbg = GET_ENTRIES_SHMID;
    if (shmctl(GET_ENTRIES_SHMID, IPC_RMID, NULL) == -1) {
        perror("shmctl");
        exit(1);
    }

    int shmid_realloc_entries = shmget(shkey_realloc_entries, sizeof(hashTableEntry) * new_capacity, IPC_CREAT | 0666);
    if (shmid_realloc_entries < 0) {
        perror("shmget");
        exit(1);
    }
    char shmid_realloc_entries_str[256];
    sprintf(shmid_realloc_entries_str, "%d", shmid_realloc_entries);
    setenv("HT_ENTRIES_SHMID", shmid_realloc_entries_str, 1);

    ht->entries = (hashTableEntry*)shmat(shmid_realloc_entries, NULL, 0);

    int32_t new_hash_prime = HT_GET_NEXT_HASH_PRIME(ht);
    for (int i = 0; i < current_capacity; i++) {
        hashTableEntry entry = tmp[i];
        if (entry.key) {
            int j = 0;
            int new_index;
            do {
                new_index = DOUBLE_HASH(entry.key, new_hash_prime, j, new_capacity);
                j++;
            } while (ht->entries[new_index].key);

            ht->entries[new_index] = entry;
        }
    }
    free(tmp);

    ht->capacity_index += 2;

    return true;
};

bool ht_size_down(hashTable* ht) {
    int32_t current_capacity = HT_GET_CAPACITY(ht);
    int32_t new_capacity =  HT_GET_PREV_CAPACITY(ht);

    hashTableEntry* tmp = calloc(current_capacity, sizeof(hashTableEntry));
    memcpy(tmp, ht->entries, current_capacity * sizeof(hashTableEntry));

    key_t shkey_realloc_entries = ftok("/tmp", 'B');

    shmdt(ht->entries);
    int dbg = GET_ENTRIES_SHMID;
    if (shmctl(GET_ENTRIES_SHMID, IPC_RMID, NULL) == -1) {
        perror("shmctl");
        exit(1);
    }

    int shmid_realloc_entries = shmget(shkey_realloc_entries, sizeof(hashTableEntry) * new_capacity, IPC_CREAT | 0666);
     if (shmid_realloc_entries < 0) {
        perror("shmget");
        exit(1);
    }
    char shmid_realloc_entries_str[256];
    sprintf(shmid_realloc_entries_str, "%d", shmid_realloc_entries);
    setenv("HT_ENTRIES_SHMID", shmid_realloc_entries_str, 1);

    ht->entries = (hashTableEntry*)shmat(shmid_realloc_entries, NULL, 0);

    int32_t new_hash_prime = HT_GET_PREV_HASH_PRIME(ht);
    for (int i = 0; i < current_capacity; i++) {
        hashTableEntry entry = tmp[i];
        if (entry.key) {
            int j = 0;
            int new_index;
            do {
                new_index = DOUBLE_HASH(entry.key, new_hash_prime, j, new_capacity);
                j++;
            } while (ht->entries[new_index].key);

            ht->entries[new_index] = entry;
        }
    }
    free(tmp);

    ht->capacity_index -= 2;

    return true;
};


void ht_print_debug(hashTable* ht) {
    if (!ht) {
        printf("Hash table is NULL\n");
        return;
    }

    printf("Hash table capacity: %d\n", HT_GET_CAPACITY(ht));
    printf("Hash table length: %d\n", ht->length);

    printf("Hash table entries:\n");
    for (int i = 0; i < HT_GET_CAPACITY(ht); i++) {
        hashTableEntry entry = ht->entries[i];
        if (entry.key) {
            printf("[%d] Key: 0x%-10zx -  Block Size: %-10u\n", i, entry.key, entry.value.block_size);
            // Add code to print stack trace if needed
        } else {
            printf("[%d] (Empty)\n", i);
        }
    }
}
