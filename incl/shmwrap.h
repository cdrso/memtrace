#ifndef SHMWRAP_H
#define SHMWRAP_H

#include <stdbool.h>
#include <sys/shm.h>

/**
 * Wrapper functions for shared memory usage
 */

// Allocates space on shared memory and returns it's shmid
int shmalloc(key_t key, size_t size );

// Loads shared memory into current context
void* shmload(int shmid);

// Frees shared memory (needs to be loaded in current context)
bool shmfree(void* ptr, int shmid);

#endif
