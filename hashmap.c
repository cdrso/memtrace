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
#include <unistd.h>

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

const static hashTableEntry clear_entry = {
    .key = 0,
    .value = {
        .block_size = 0,
        .stack_trace = {0, 0, 0, 0, 0, 0, 0, 0, 0, 0}
    }
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

#define GET_HT_SHMID atoi(getenv("HT_SHMID"))

/**
 * Can't load context without checking the mutex
 * Can't check mutext without loading the context
 * what do?
 */

void ht_load_context(hashTable* ht) {
    /**
     * Take mutex without loading the context through shmat
     * when taken then you can load the context
     * do not free, leave it to be used and freed by caller function
     */

    pthread_mutex_t* process_mutex = (pthread_mutex_t*)shmat(ht->mutex_shmid, NULL, 0);
    if (process_mutex == (void*)-1) {
        perror("shmat");
        exit(1);
    }
    pthread_mutex_lock(process_mutex);


    hashTableEntry* process_entries = (hashTableEntry*)shmat(ht->entries_shmid, NULL, 0);
    if (process_entries == (void*)-1) {
        perror("shmat");
        exit(1);
    }

    ht->entries = process_entries;
    ht->mutex = process_mutex;
}

hashTable* ht_create() {
    key_t shkey_ht = ftok("/tmp", 'A');
    int shmid_ht = shmget(shkey_ht, sizeof(hashTable), IPC_CREAT | 0666);
     if (shmid_ht < 0) {
        perror("shmget");
        exit(1);
    }
    hashTable* ht = (hashTable*)shmat(shmid_ht, NULL, 0);
    if (ht == (void*)-1) {
        perror("shmat");
        exit(1);
    }
    char shmid_ht_str[256];
    sprintf(shmid_ht_str, "%d", shmid_ht);
    setenv("HT_SHMID", shmid_ht_str, 1);

    ht->length = 0;
    ht->capacity_index = INITIAL_CAPACITY_INDEX;
    uint32_t capacity = HT_GET_CAPACITY(ht);

    key_t shkey_entries = ftok("/tmp", 'B');
    int shmid_entries = shmget(shkey_entries, sizeof(hashTableEntry) * HT_GET_CAPACITY(ht), IPC_CREAT | 0666);
     if (shmid_entries < 0) {
        perror("shmget");
        exit(1);
    }
    hashTableEntry* ht_entries = (hashTableEntry*)shmat(shmid_entries, NULL, 0);
    if (ht_entries == (void*)-1) {
        perror("shmat");
        exit(1);
    }
    ht->entries_shmid = shmid_entries;
    ht->entries = ht_entries;

    for (int i = 0; i < capacity; i++) {
        ht->entries[i] = clear_entry;
    }

    key_t shkey_mutex = ftok("/tmp", 'C');
    int shmid_mutex = shmget(shkey_mutex, sizeof(pthread_mutex_t), IPC_CREAT | 0666);
     if (shmid_mutex < 0) {
        perror("shmget");
        exit(1);
    }
    pthread_mutex_t* ht_mutex = (pthread_mutex_t*)shmat(shmid_mutex, NULL, 0);
    if (ht_mutex == (void*)-1) {
        perror("shmat");
        exit(1);
    }

    if (pthread_mutex_init(ht_mutex, NULL)!= 0) {
        perror("Mutex initialization failed");
        exit(EXIT_FAILURE);
    }

    ht->mutex_shmid = shmid_mutex;
    ht->mutex = ht_mutex;


    return ht;
}

void ht_destroy(hashTable* ht) {
    if (!ht) { return; }
    //careful, not checking if ht is in use

    /*
    pthread_mutex_t* process_mutex = (pthread_mutex_t*)shmat(ht->mutex_shmid, NULL, 0);
    if (process_mutex == (void*)-1) {
        perror("shmat");
        exit(1);
    }

    while (1) {
        if (pthread_mutex_trylock(process_mutex) != 0) {
            break;
        }
        usleep(10000);
    }
    */

    shmdt(ht->entries);
    if (shmctl(ht->entries_shmid, IPC_RMID, NULL) == -1) {
        perror("shmctl");
        exit(1);
    }

    if (pthread_mutex_destroy(ht->mutex)!= 0) {
        perror("Mutex destruction failed");
        exit(EXIT_FAILURE);
    }
    shmdt(ht->mutex);
    if (shmctl(ht->mutex_shmid, IPC_RMID, NULL) == -1) {
        perror("shmctl");
        exit(1);
    }

    shmdt(ht);
    if (shmctl(GET_HT_SHMID, IPC_RMID, NULL) == -1) {
        perror("shmctl");
        exit(1);
    }
}

bool ht_insert(hashTable* ht, const size_t key, const allocInfo value) {
    if (!ht) {
        printf("ht input is NULL\n");
        return false;
    }

    ht_load_context(ht);

    uint32_t capacity = HT_GET_CAPACITY(ht);
    uint32_t hash_prime = HT_GET_HASH_PRIME(ht);

    int i = 0;
    int index;
    do {
        index = DOUBLE_HASH(key, hash_prime, i, capacity);
        i++;
    } while (ht->entries[index].key && ht->entries[index].key != key);

    hashTableEntry entry = {
        .key = key,
        .value = value
    };

    if (ht->entries[index].key != key) {
    ht->length++;
    }

    ht->entries[index] = entry;

    if (((float)ht->length / capacity) >= RESIZE_UP_FACTOR && (ht->capacity_index < LAST_CAPACITY_INDEX)) {
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

    ht_load_context(ht);

    uint32_t capacity = HT_GET_CAPACITY(ht);
    uint32_t hash_prime = HT_GET_HASH_PRIME(ht);

    // Can´t use ht_get because the index is needed to set that entry to NULL

    int i = 0;
    int index;
    int start_index = DOUBLE_HASH(key, hash_prime, 0, capacity);
    size_t found_key = -1;
    do {
        index = DOUBLE_HASH(key, hash_prime, i, capacity);
        if (ht->entries[index].key) {
            found_key = ht->entries[index].key;
        }
        if ((i > 0) && (index == start_index)) {
            pthread_mutex_unlock(ht->mutex);
            return false;
        }
        i++;
    } while (found_key != key);

    hashTableEntry clear_entry = {
        .key = 0,
        .value = {
            .block_size = 0,
            .stack_trace = {0, 0, 0, 0, 0, 0, 0, 0, 0, 0,}
        }
    };
    ht->entries[index] = clear_entry;

    ht->length--;

    if (((float)ht->length / capacity) <= RESIZE_DOWN_FACTOR && (ht->capacity_index > INITIAL_CAPACITY_INDEX)) {
        ht_size_down(ht);
    }

    pthread_mutex_unlock(ht->mutex);

    return true;
}

allocInfo* ht_get(hashTable* ht, const size_t key) {
    if (!ht) { return NULL; }

    ht_load_context(ht);

    uint32_t capacity = HT_GET_CAPACITY(ht);
    uint32_t hash_prime = HT_GET_HASH_PRIME(ht);

    int i = 0;
    int index;
    int start_index = DOUBLE_HASH(key, hash_prime, 0, capacity);
    size_t found_key = -1;
    do {
        index = DOUBLE_HASH(key, hash_prime, i, capacity);
        if (ht->entries[index].key) {
            found_key = ht->entries[index].key;
        }
        if ((i > 0) && (index == start_index)) {
            pthread_mutex_unlock(ht->mutex);
            return NULL;
        }
        i++;
    } while (found_key != key);

    allocInfo* ret = &ht->entries[index].value;

    pthread_mutex_unlock(ht->mutex);

    return ret;
}

__attribute__((weak)) void* no_override_calloc(size_t num_elements, size_t element_size) { return NULL; }
__attribute__((weak)) void  no_override_free(void* ptr) { return ; }

//TODO no_override_calloc & no_override_free __attribute((weak))__
bool ht_size_up(hashTable* ht) {
    int32_t current_capacity = HT_GET_CAPACITY(ht);
    int32_t new_capacity =  HT_GET_NEXT_CAPACITY(ht);

    #ifdef HT_TEST
    hashTableEntry* tmp = calloc(current_capacity, sizeof(hashTableEntry));
    #else
    hashTableEntry* tmp = no_override_calloc(current_capacity, sizeof(hashTableEntry));
    #endif
    memcpy(tmp, ht->entries, current_capacity * sizeof(hashTableEntry));


    /* same parameters as parent process gives same id */
    key_t shkey_realloc_entries = ftok("/tmp", 'B');

    shmdt(ht->entries);
    if (shmctl(ht->entries_shmid, IPC_RMID, NULL) == -1) {
        perror("shmctl");
        exit(1);
    }

    int shmid_realloc_entries = shmget(shkey_realloc_entries, sizeof(hashTableEntry) * new_capacity, IPC_CREAT | 0666);
    if (shmid_realloc_entries < 0) {
        perror("shmget");
        exit(1);
    }

    ht->entries_shmid = shmid_realloc_entries;
    ht->entries = (hashTableEntry*)shmat(shmid_realloc_entries, NULL, 0);

    for (int i = 0; i < new_capacity; i++) {
        ht->entries[i] = clear_entry;
    }

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
    #ifdef HT_TEST
    free(tmp);
    #else
    no_override_free(tmp);
    #endif

    ht->capacity_index += 2;

    return true;
};

bool ht_size_down(hashTable* ht) {
    int32_t current_capacity = HT_GET_CAPACITY(ht);
    int32_t new_capacity =  HT_GET_PREV_CAPACITY(ht);

    #ifdef HT_TEST
    hashTableEntry* tmp = calloc(current_capacity, sizeof(hashTableEntry));
    #else
    hashTableEntry* tmp = no_override_calloc(current_capacity, sizeof(hashTableEntry));
    #endif
    memcpy(tmp, ht->entries, current_capacity * sizeof(hashTableEntry));

    key_t shkey_realloc_entries = ftok("/tmp", 'B');

    shmdt(ht->entries);
    if (shmctl(ht->entries_shmid, IPC_RMID, NULL) == -1) {
        perror("shmctl");
        exit(1);
    }

    int shmid_realloc_entries = shmget(shkey_realloc_entries, sizeof(hashTableEntry) * new_capacity, IPC_CREAT | 0666);
     if (shmid_realloc_entries < 0) {
        perror("shmget");
        exit(1);
    }

    ht->entries_shmid = shmid_realloc_entries;
    ht->entries = (hashTableEntry*)shmat(shmid_realloc_entries, NULL, 0);

    for (int i = 0; i < new_capacity; i++) {
        ht->entries[i] = clear_entry;
    }

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
    #ifdef HT_TEST
    free(tmp);
    #else
    no_override_free(tmp);
    #endif

    ht->capacity_index -= 2;

    return true;
};


void ht_print_debug(hashTable* ht) {
    if (!ht) {
        printf("Hash table is NULL\n");
        return;
    }

    ht_load_context(ht);

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

    pthread_mutex_unlock(ht->mutex);
}
