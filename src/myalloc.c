#ifdef RUNTIME
#define _GNU_SOURCE
#include <execinfo.h>
#include <stdio.h>
#include <dlfcn.h>
#include <stdlib.h>
#include <sys/ipc.h>
#include <sys/shm.h>
#include "hashmap.h"
#include "shmwrap.h"
#include <pthread.h>

static void*(*glibc_malloc)(size_t size);
static void*(*glibc_calloc)(size_t num_elements, size_t element_size);
static void*(*glibc_realloc)(void* ptr, size_t new_size);
static void*(*glibc_free)(void*);

static char first_malloc_intercept = 1;
static char first_free_intercept = 1;
static char first_calloc_intercept = 1;
static char first_realloc_intercept = 1;

#define GET_HT_SHMID atoi(getenv("HT_SHMID"))

void* malloc(size_t size) {
    if (first_malloc_intercept) {
        glibc_malloc = dlsym(RTLD_NEXT, "malloc");
        char* error;
        if ((error = dlerror()) != NULL) {
            fputs(error, stderr);
            exit(1);
        }

        first_malloc_intercept = 0;

        void* ptr = glibc_malloc(size);

        hashTable* ht = shmload(GET_HT_SHMID);
        allocInfo trace = {
            .block_size = size,
        };
        (void)backtrace(trace.stack_trace, 10);

        ht_insert(ht, (size_t)ptr, trace);

        first_malloc_intercept = 1;

        return ptr;
    }

    void* ptr = glibc_malloc(size);

    return ptr;
}

void* calloc(size_t num_elements, size_t element_size) {
    if (first_calloc_intercept) {
        glibc_calloc = dlsym(RTLD_NEXT, "calloc");
        char* error;
        if ((error = dlerror()) != NULL) {
            fputs(error, stderr);
            exit(1);
        }

        first_calloc_intercept = 0;
        void* ptr = glibc_calloc(num_elements, element_size);

        hashTable* ht = shmload(GET_HT_SHMID);

        allocInfo trace = {
            .block_size = num_elements * element_size,
        };
        (void)backtrace(trace.stack_trace, 10);

        ht_insert(ht, (size_t)ptr, trace);

        first_calloc_intercept = 1;

        return ptr;
    }

    void* ptr = glibc_calloc(num_elements, element_size);

    return ptr;
}

void* realloc(void* ptr, size_t new_size) {
    if (first_realloc_intercept) {
        glibc_realloc = dlsym(RTLD_NEXT, "realloc");
        char* error;
        if ((error = dlerror()) != NULL) {
            fputs(error, stderr);
            exit(1);
        }

        first_realloc_intercept = 0;
        void* new_ptr = glibc_realloc(ptr, new_size);

        hashTable* ht = shmload(GET_HT_SHMID);

        allocInfo trace = {
            .block_size = new_size,
        };
        (void)backtrace(trace.stack_trace, 10);

        ht_delete(ht, (size_t)ptr);
        ht_insert(ht, (size_t)new_ptr, trace);

        first_realloc_intercept = 1;

        return new_ptr;
    }

    void* new_ptr = glibc_realloc(ptr, new_size);

    return new_ptr;
}

void free(void* ptr) {
    if (!ptr) { return; }

    if (first_free_intercept) {
        glibc_free = dlsym(RTLD_NEXT, "free");
        char* error;
        if ((error = dlerror()) != NULL) {
            fputs(error, stderr);
            exit(1);
        }

        first_free_intercept = 0;

        hashTable* ht = shmload(GET_HT_SHMID);

        ht_delete(ht, (size_t)ptr);

        first_free_intercept = 1;
    }

    glibc_free(ptr);
}

void* no_intercept_calloc(size_t num_elements, size_t element_size) {
    glibc_calloc = dlsym(RTLD_NEXT, "calloc");
    char* error;
    if ((error = dlerror()) != NULL) {
        fputs(error, stderr);
        exit(1);
    }

    void* ptr = glibc_calloc(num_elements, element_size);
    return ptr;
}

void  no_intercept_free(void* ptr) {
    glibc_free = dlsym(RTLD_NEXT, "free");
    char* error;
    if ((error = dlerror()) != NULL) {
        fputs(error, stderr);
        exit(1);
    }
    glibc_free(ptr);
}

#endif
