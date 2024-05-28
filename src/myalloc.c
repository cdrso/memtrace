#ifdef RUNTIME
#define _GNU_SOURCE
#include <execinfo.h>
#include <stdio.h>
#include <stdint.h>
#include <dlfcn.h>
#include <stdlib.h>
#include <sys/ipc.h>
#include <sys/shm.h>
#include <pthread.h>
#include "hashmap.h"
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

        hashTable* ht = shmload(GET_HT_SHMID);
        allocInfo trace = {
            .block_size = size,
        };
        (void)backtrace(trace.stack_trace, 10);

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

        hashTable* ht = shmload(GET_HT_SHMID);

        allocInfo trace = {
            .block_size = num_elements * element_size,
        };
        (void)backtrace(trace.stack_trace, 10);

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

        hashTable* ht = shmload(GET_HT_SHMID);

        allocInfo trace = {
            .block_size = new_size,
        };
        (void)backtrace(trace.stack_trace, 10);

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

#endif
