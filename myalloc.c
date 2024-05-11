#ifdef RUNTIME
#define _GNU_SOURCE
#include <execinfo.h>
#include <stdio.h>
#include <dlfcn.h>
#include <stdlib.h>
#include <sys/ipc.h>
#include <sys/shm.h>
#include "hashmap.h"

static void*(*glibc_malloc)(size_t size);
static void*(*glibc_calloc)(size_t num_elements, size_t element_size);
static void*(*glibc_realloc)(void* ptr, size_t new_size);
static void*(*glibc_free)(void*);

static char first_malloc_intercept = 1;
static char first_free_intercept = 1;
static char first_calloc_intercept = 1;
static char first_realloc_intercept = 1;

#define GET_HT_SHMID atoi(getenv("HT_SHMID"))
#define GET_ENTRIES_SHMID atoi(getenv("HT_ENTRIES_SHMID"))

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

        int ht_shmid = GET_HT_SHMID;
        int entries_shmid = GET_ENTRIES_SHMID;

        hashTable* ht = (hashTable*)shmat(ht_shmid, NULL, 0);
        if (ht == (void*)-1) {
            perror("shmat");
            exit(1);
        }
        hashTableEntry* process_entries = (hashTableEntry*)shmat(entries_shmid, NULL, 0);
        ht->entries = process_entries;

        allocInfo trace = {
            .block_size = size,
        };
        (void)backtrace(trace.stack_trace, 10);

        printf("malloc(%zu) = %p\n", size, ptr);

        ht_insert(ht, (size_t)ptr, trace);

        ht_print_debug(ht);

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

        int ht_shmid = GET_HT_SHMID;
        int entries_shmid = GET_ENTRIES_SHMID;

        hashTable* ht = (hashTable*)shmat(ht_shmid, NULL, 0);
        if (ht == (void*)-1) {
            perror("shmat");
            exit(1);
        }
        hashTableEntry* process_entries = (hashTableEntry*)shmat(entries_shmid, NULL, 0);
        ht->entries = process_entries;

        allocInfo trace = {
            .block_size = num_elements * element_size,
        };
        (void)backtrace(trace.stack_trace, 10);

        printf("calloc(%zu, %zu) = %p\n", num_elements, element_size, ptr);
        ht_insert(ht, (size_t)ptr, trace);

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

        int ht_shmid = GET_HT_SHMID;
        int entries_shmid = GET_ENTRIES_SHMID;

        hashTable* ht = (hashTable*)shmat(ht_shmid, NULL, 0);
        if (ht == (void*)-1) {
            perror("shmat");
            exit(1);
        }
        hashTableEntry* process_entries = (hashTableEntry*)shmat(entries_shmid, NULL, 0);
        ht->entries = process_entries;

        allocInfo trace = {
            .block_size = new_size,
        };
        (void)backtrace(trace.stack_trace, 10);

        printf("realloc(%p, %zu) = %p\n", ptr, new_size, new_ptr);
        ht_delete(ht, (size_t)ptr);
        ht_insert(ht, (size_t)new_ptr, trace);

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

        int ht_shmid = GET_HT_SHMID;
        int entries_shmid = GET_ENTRIES_SHMID;

        hashTable* ht = (hashTable*)shmat(ht_shmid, NULL, 0);
        if (ht == (void*)-1) {
            perror("shmat");
            exit(1);
        }
        hashTableEntry* process_entries = (hashTableEntry*)shmat(entries_shmid, NULL, 0);
        ht->entries = process_entries;

        printf("free(%p)\n", ptr);
        ht_delete(ht, (size_t)ptr);
    }

    glibc_free(ptr);
}

#endif
