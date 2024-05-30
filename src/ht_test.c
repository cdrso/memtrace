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
 * File: ht_test.c
 * Author: Alejandro Cadarso
 * Date: 28-05-2024
 *
 * This file contains test cases for the hash table implementation used
 * in Memtrace. It exercises various aspects of the hash table including
 * insertion, deletion, retrieval, and debugging output. The test
 * demonstrates the hash table's ability to handle dynamic resizing,
 * collision resolution via double hashing, and the correct handling of
 * different block sizes for allocated memory. This testing helps ensure
 * the reliability and correctness of the hash table's performance in
 * tracking memory allocations and deallocations within the Memtrace
 * framework.
 *
 */

#include <assert.h>
#include "hashtable.h"

#define NUM_ALLOCATIONS 1000
#define OVERWRITE_KEY 33

const allocInfo mock_1 = {
    .block_size = 1,
};
const allocInfo mock_2 = {
    .block_size = 2,
};

int main(void) {
    hashTable* ht = ht_create();

    for (int i = 1; i < NUM_ALLOCATIONS; i++) {
        assert(ht_insert(ht, i, mock_1));
    }
    for (int i = 1; i < NUM_ALLOCATIONS/2; i++) {
        assert(ht_delete(ht, i));
    }
    for (int i = 1; i < NUM_ALLOCATIONS/2; i++) {
        assert(!ht_get(ht, i));
    }
    for (int i = NUM_ALLOCATIONS/2; i < NUM_ALLOCATIONS; i++) {
        assert(ht_get(ht, i));
    }
    for (int i = NUM_ALLOCATIONS/2; i < NUM_ALLOCATIONS; i++) {
        assert(ht_delete(ht, i));
    }
    for (int i = 1; i < NUM_ALLOCATIONS; i++) {
        assert(!ht_get(ht, i));
    }

    ht_insert(ht, OVERWRITE_KEY, mock_1);
    ht_insert(ht, OVERWRITE_KEY, mock_2);

    const allocInfo* overwrite_entry = ht_get(ht, OVERWRITE_KEY);
    assert( overwrite_entry->block_size == mock_2.block_size);

    ht_destroy(ht);

    return 0;
}
