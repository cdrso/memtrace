#include <stdlib.h>
#include <stdio.h>
#include "shmwrap.h"

int shmalloc(key_t key, size_t size ) {
    int shmid = shmget(key, size, IPC_CREAT | 0666);
    return shmid;
}

void* shmload(int shmid) {
    void* ptr = shmat(shmid, NULL, 0);
    if (ptr == (void*)-1) {
        return NULL;
    }

    return ptr;
}

bool shmfree(void* ptr, int shmid) {
    shmdt(ptr);
    if (shmctl(shmid, IPC_RMID, NULL) == -1) {
        return false;
    }

    return true;
}
