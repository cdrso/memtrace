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

#include <stdio.h>
#include "hashtable.h"

#define NUM_ALLOCATIONS 1000

allocInfo mock = {
    .block_size = 1,
};
allocInfo mock1 = {
    .block_size = 2,
};

int main(void) {
    hashTable* ht = ht_create();

    for (int i = 1; i < NUM_ALLOCATIONS; i++) {
        if (!ht_insert(ht, i, mock)) {
            printf("w_pa\n");
        }
    }
    for (int i = 1; i < NUM_ALLOCATIONS/2; i++) {
        if(!ht_delete(ht, i)) {
            printf("wepa\n");
        }
    }
    for (int i = 1; i < NUM_ALLOCATIONS/2; i++) {
        if(ht_get(ht, i)) {
            printf("wupa\n");
        }
    }
    for (int i = NUM_ALLOCATIONS/2; i < NUM_ALLOCATIONS; i++) {
        if(!ht_get(ht, i)) {
            printf("wopa\n");
        }
    }
    for (int i = NUM_ALLOCATIONS/2; i < NUM_ALLOCATIONS; i++) {
        if(!ht_delete(ht, i)) {
            printf("wepa\n");
        }
    }
    for (int i = 1; i < NUM_ALLOCATIONS; i++) {
        if(ht_get(ht, i)) {
            printf("wapa\n");
        }
    }

    ht_insert(ht, 33, mock);
    ht_insert(ht, 33, mock1);
    ht_insert(ht, 33, mock);


    ht_print_debug(ht);
    ht_destroy(ht);

    return 0;
}
