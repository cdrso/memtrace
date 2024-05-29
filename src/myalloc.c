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
 * File: myalloc.c
 * Author: Alejandro Cadarso
 * Date: 28-05-2024
 *
 * This file implements a shared library for the Memtrace memory profiling tool.
 * It intercepts standard library memory allocation and deallocation functions
 * (`malloc`, `calloc`, `realloc`, `free`) using dynamic linking to track
 * memory operations performed by the target application. The interception
 * mechanism ensures that every memory operation is recorded in a shared
 * memory hash table, allowing the parent process to collect and analyze memory
 * profiles.
 *
 */

#ifdef RUNTIME
#define _GNU_SOURCE
#include <execinfo.h>
#include <stdio.h>
#include <stdint.h>
#include <dlfcn.h>
#include <stdlib.h>
#include <string.h>
#include <sys/ipc.h>
#include <sys/shm.h>
#include <pthread.h>
#include "hashtable.h"
#include "shmwrap.h"


// Pointers to stdlib functions
static void* (*libc_malloc)(size_t size);
static void* (*libc_calloc)(size_t num_elements, size_t element_size);
static void* (*libc_realloc)(void* ptr, size_t new_size);
static void  (*libc_free)(void*);


// Intercept flags needed to prevent recursive interceptions
static uint8_t intercept_flags =    0x0F;
#define FIRST_MALLOC_INTERCEPT      0X01 // 0000 0001
#define FIRST_CALLOC_INTERCEPT      0X02 // 0000 0010
#define FIRST_REALLOC_INTERCEPT     0X04 // 0000 0100
#define FIRST_FREE_INTERCEPT        0X08 // 0000 1000


#define GET_HT_SHMID atoi(getenv("HT_SHMID"))


#define BT_OFFSET 2

void _add_trace_symbols(allocInfo* trace);

/**
 * If ht functions fail then exit(1) and parent process
 * will call ht_destroy, no need to free resources here
 */

void* malloc(size_t size) {
    if (intercept_flags & FIRST_MALLOC_INTERCEPT) {
        libc_malloc = dlsym(RTLD_NEXT, "malloc");
        char* error;
        if ((error = dlerror()) != NULL) {
            fputs(error, stderr);
            exit(1);
        }

        intercept_flags &= ~FIRST_MALLOC_INTERCEPT;

        void* ptr = libc_malloc(size);
        if (!ptr) {
            return NULL;
        }

        hashTable* ht = shmload(GET_HT_SHMID);
        allocInfo trace = {
            .block_size = size,
        };
        _add_trace_symbols(&trace);

        if (!ht_insert(ht, (size_t)ptr, trace)) {
            fputs("Unrecoverable error: HashTable | Shared Memory Failure\n", stderr);
            fputs("Error at insert\n", stderr);
            exit(1);
        }

        intercept_flags |= FIRST_MALLOC_INTERCEPT;

        return ptr;
    }

    void* ptr = libc_malloc(size);

    return ptr;
}

void* calloc(size_t num_elements, size_t element_size) {
    if (intercept_flags & FIRST_CALLOC_INTERCEPT) {
        libc_calloc = dlsym(RTLD_NEXT, "calloc");
        char* error;
        if ((error = dlerror()) != NULL) {
            fputs(error, stderr);
            exit(1);
        }

        intercept_flags &= ~FIRST_CALLOC_INTERCEPT;
        void* ptr = libc_calloc(num_elements, element_size);
        if (!ptr) {
            return NULL;
        }

        hashTable* ht = shmload(GET_HT_SHMID);

        allocInfo trace = {
            .block_size = num_elements * element_size,
        };
        _add_trace_symbols(&trace);

        if (!ht_insert(ht, (size_t)ptr, trace)) {
            fputs("Unrecoverable error: HashTable | Shared Memory Failure\n", stderr);
            fputs("Error at insert\n", stderr);
            exit(1);
        }

        intercept_flags |= FIRST_CALLOC_INTERCEPT;

        return ptr;
    }

    void* ptr = libc_calloc(num_elements, element_size);

    return ptr;
}

void* realloc(void* ptr, size_t new_size) {
    if (intercept_flags & FIRST_REALLOC_INTERCEPT) {
        libc_realloc = dlsym(RTLD_NEXT, "realloc");
        char* error;
        if ((error = dlerror()) != NULL) {
            fputs(error, stderr);
            exit(1);
        }

        intercept_flags &= ~FIRST_REALLOC_INTERCEPT;
        void* new_ptr = libc_realloc(ptr, new_size);
        if (!new_ptr) {
            return NULL;
        }

        hashTable* ht = shmload(GET_HT_SHMID);

        allocInfo trace = {
            .block_size = new_size,
        };
        _add_trace_symbols(&trace);

        if (!ht_delete(ht, (size_t)ptr) || !ht_insert(ht, (size_t)ptr, trace)) {
            fputs("Unrecoverable error: HashTable | Shared Memory Failure\n", stderr);
            exit(1);
        }

        intercept_flags |= FIRST_REALLOC_INTERCEPT;

        return new_ptr;
    }

    void* new_ptr = libc_realloc(ptr, new_size);

    return new_ptr;
}

void free(void* ptr) {
    if (!ptr) { return; }

    if (intercept_flags & FIRST_FREE_INTERCEPT) {
        libc_free = dlsym(RTLD_NEXT, "free");
        char* error;
        if ((error = dlerror()) != NULL) {
            fputs(error, stderr);
            exit(1);
        }

        intercept_flags &= ~FIRST_FREE_INTERCEPT;

        hashTable* ht = shmload(GET_HT_SHMID);

        if (!ht_delete(ht, (size_t)ptr)) {
            fputs("Unrecoverable error: HashTable | Shared Memory Failure\n", stderr);
            fputs("Error at free\n", stderr);
            exit(1);
        }

        intercept_flags |= FIRST_FREE_INTERCEPT;
    }

    libc_free(ptr);
}

void* _no_intercept_calloc(size_t num_elements, size_t element_size) {
    libc_calloc = dlsym(RTLD_NEXT, "calloc");
    char* error;
    if ((error = dlerror()) != NULL) {
        fputs(error, stderr);
        exit(1);
    }

    void* ptr = libc_calloc(num_elements, element_size);
    return ptr;
}

void  _no_intercept_free(void* ptr) {
    libc_free = dlsym(RTLD_NEXT, "free");
    char* error;
    if ((error = dlerror()) != NULL) {
        fputs(error, stderr);
        exit(1);
    }
    libc_free(ptr);
}

void _add_trace_symbols(allocInfo* trace) {
    char tmp_buffer[MAX_CHAR];

    int nptrs;
    void* buffer[MAX_CHAR];
    char** strings;

    nptrs = backtrace(buffer, MAX_CHAR);

    strings = backtrace_symbols(buffer, nptrs);

    for (int i = BT_OFFSET; i < nptrs && i < MAX_STRINGS; i++) {
        strncpy(tmp_buffer, strings[i], MAX_CHAR - 1);
        tmp_buffer[MAX_CHAR - 1] = '\0';
        strcpy(trace->stack_trace[i], tmp_buffer);
    }

    free(strings);
}

#endif
