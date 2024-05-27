#ifndef HASHMAP_H
#define HASHMAP_H

#include <stdbool.h>
#include <stdint.h>
#include <semaphore.h>

/**
 * dynamic resizing
 * double hashing
 * fnv-1
 */

typedef struct allocInfo {
    uint32_t block_size;
    void* stack_trace[10];
} allocInfo;

typedef struct hashTableEntry {
    size_t key;
    allocInfo value;
} hashTableEntry;

/**
 * The hashtable capacity is not stored direcly,
 * instead it can be retrieved with the HT_GET_CAPACITY macro
 * this functionality is not public
 */
typedef struct hashTable {
    uint32_t capacity_index;
    uint32_t length;
    int entries_shmid;
    hashTableEntry* entries;
    int mutex_shmid;
    pthread_mutex_t* mutex;
} hashTable;


// Creates a hashtable and returns a pointer
hashTable* ht_create();

// Destroys a hashtable, no return
void ht_destroy(hashTable* ht);

// Inserts entry into a hashtable, true is success false if failure
bool ht_insert(hashTable* ht, const size_t key, allocInfo value);

// Deletes entry from a hashtable, true is success false if failure
bool ht_delete(hashTable* ht, const size_t key);

// Retrieves allocationInfo from a hashtable, returns a const pointer
const allocInfo* ht_get(hashTable* ht, const size_t key);

// Prints hashtable contents for dbg purposes
void ht_print_debug(hashTable* ht);

#endif
