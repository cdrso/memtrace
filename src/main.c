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
 * File: main.c
 * Author: Alejandro Cadarso
 * Date: 28-05-2024
 *
 * This file serves as an example application for testing the Memtrace
 * memory profiling tool. It dynamically allocates and deallocates a
 * series of blocks of memory with varying sizes.
 *
 */

#include <stdio.h>
#include <stdlib.h>
#include <pthread.h>

#define NUM_ALLOCATIONS 50
#define MAX_ALLOCATION_SIZE 1024

int main(void) {
    void* arr[NUM_ALLOCATIONS];

    for (int i = 0; i < NUM_ALLOCATIONS; i++) {
        size_t size = (rand() % MAX_ALLOCATION_SIZE) + 1;
        arr[i] = malloc(size);
        if (arr[i] == NULL) {
            fprintf(stderr, "Failed to allocate memory\n");
            return -1;
        }
    }
    for (int i = 0; i < NUM_ALLOCATIONS-1; i++) {
        free(arr[i]);
    }

    return 0;
}
