#include <stdio.h>
#include <stdlib.h>
#include <stdbool.h>
#include <stdint.h>

/**
 * dynamic resizing
 * double hashing
 * fnv-1
 */

typedef struct allocInfo {
    uint32_t block_size;
    void* stack_trace[10];
} allocInfo;

typedef struct hashTableEntry hashTableEntry;

/**
 * The hashtable capacity is not stored direcly,
 * instead it can be retrieved with the HT_GET_CAPACITY macro
 */
typedef struct hashTable {
    hashTableEntry* entries;
    int32_t capacity_index;
    int32_t length;
} hashTable;

typedef struct hashTableEntry {
    size_t key;
    allocInfo value;
} hashTableEntry;

// Creates a hashtable and returns a pointer
bool ht_create(hashTable* ht, hashTableEntry* entries);

// Destroys a hashtable, no return
void ht_destroy(hashTable* ht);

// Inserts entry into a hashtable, true is success false if failure
bool ht_insert(hashTable* ht, const size_t key, allocInfo value);

// Deletes entry from a hashtable, true is success false if failure
bool ht_delete(hashTable* ht, const size_t key);

// Retrieves allocationInfo from a hashtable, returns a pointer
allocInfo* ht_get(hashTable* ht, const size_t key);

// Prints hashtable contents for dbg purposes
void ht_print_debug(hashTable* ht);





