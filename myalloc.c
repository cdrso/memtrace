#ifdef RUNTIME
#define _GNU_SOURCE
#include <execinfo.h>
#include <stdio.h>
#include <dlfcn.h>
#include <stdlib.h>
#include <sys/ipc.h>
#include <sys/shm.h>
#include "hashmap.h"
#include <pthread.h>

static void*(*libc_malloc)(size_t size);
static void*(*libc_calloc)(size_t num_elements, size_t element_size);
static void*(*libc_realloc)(void* ptr, size_t new_size);
static void*(*libc_free)(void*);

static char first_malloc_intercept = 1;
static char first_free_intercept = 1;
static char first_calloc_intercept = 1;
static char first_realloc_intercept = 1;

void* malloc(size_t size) {
    if (first_malloc_intercept) {
        libc_malloc = dlsym(RTLD_NEXT, "malloc");
        char* error;
        if ((error = dlerror()) != NULL) {
            fputs(error, stderr);
            exit(1);
        }

        first_malloc_intercept = 0;

        void* ptr = libc_malloc(size);

        allocInfo trace = {
            .block_size = size,
        };
        (void)backtrace(trace.stack_trace, 10);

        ht_insert(ht, (size_t)ptr, trace);

        printf("malloc(%ld) = %p\n", size, ptr);

        first_malloc_intercept = 1;

        return ptr;
    }

    void* ptr = libc_malloc(size);

    return ptr;
}

void* calloc(size_t num_elements, size_t element_size) {
    if (first_calloc_intercept) {
        libc_calloc = dlsym(RTLD_NEXT, "calloc");
        char* error;
        if ((error = dlerror()) != NULL) {
            fputs(error, stderr);
            exit(1);
        }

        first_calloc_intercept = 0;
        void* ptr = libc_calloc(num_elements, element_size);

        allocInfo trace = {
            .block_size = num_elements * element_size,
        };
        (void)backtrace(trace.stack_trace, 10);

        ht_insert(ht, (size_t)ptr, trace);
        printf("calloc(%ld, %ld) = %p\n", num_elements, element_size, ptr);

        first_calloc_intercept = 1;

        return ptr;
    }

    void* ptr = libc_calloc(num_elements, element_size);

    return ptr;
}

void* realloc(void* ptr, size_t new_size) {
    if (first_realloc_intercept) {
        libc_realloc = dlsym(RTLD_NEXT, "realloc");
        char* error;
        if ((error = dlerror()) != NULL) {
            fputs(error, stderr);
            exit(1);
        }

        first_realloc_intercept = 0;
        void* new_ptr = libc_realloc(ptr, new_size);

        allocInfo trace = {
            .block_size = new_size,
        };
        (void)backtrace(trace.stack_trace, 10);

        ht_delete(ht, (size_t)ptr);
        ht_insert(ht, (size_t)new_ptr, trace);
        printf("realloc(%p, %ld) = %p\n", ptr, new_size, new_ptr);

        first_realloc_intercept = 1;

        return new_ptr;
    }

    void* new_ptr = libc_realloc(ptr, new_size);

    return new_ptr;
}

void free(void* ptr) {
    if (!ptr) { return; }

    if (first_free_intercept) {
        libc_free = dlsym(RTLD_NEXT, "free");
        char* error;
        if ((error = dlerror()) != NULL) {
            fputs(error, stderr);
            exit(1);
        }

        first_free_intercept = 0;

        ht_delete(ht, (size_t)ptr);
        printf("free(%p)\n", ptr);

        first_free_intercept = 1;
    }

    libc_free(ptr);
}

void* no_intercept_calloc(size_t num_elements, size_t element_size) {
    libc_calloc = dlsym(RTLD_NEXT, "calloc");
    char* error;
    if ((error = dlerror()) != NULL) {
        fputs(error, stderr);
        exit(1);
    }

    void* ptr = libc_calloc(num_elements, element_size);
    return ptr;
}

void* no_intercept_malloc(size_t size) {
    libc_malloc = dlsym(RTLD_NEXT, "malloc");
    char* error;
    if ((error = dlerror()) != NULL) {
        fputs(error, stderr);
        exit(1);
    }

    void* ptr = libc_malloc(size);
    return ptr;
}

void  no_intercept_free(void* ptr) {
    libc_free = dlsym(RTLD_NEXT, "free");
    char* error;
    if ((error = dlerror()) != NULL) {
        fputs(error, stderr);
        exit(1);
    }
    libc_free(ptr);
}

#endif
