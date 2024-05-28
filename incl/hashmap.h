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
 * File: hashmap.h
 * Author: Alejandro Cadarso
 * Date: 28-05-2024
 *
 * This header file provides the interface for a custom hash table
 * implementation in C, featuring dynamic resizing, double hashing for
 * collision resolution, and the FNV-1 hash function. It includes structures
 * for hash table entries and allocation information, alongside function
 * prototypes for creating, inserting, deleting, retrieving, and debugging
 * hash table operations
 *
 */

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
