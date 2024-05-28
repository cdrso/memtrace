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
 * File: shmwrap.c
 * Author: Alejandro Cadarso
 * Date: 28-05-2024
 *
 * This file provides a wrapper around system-shared memory (sys/shm)
 * functionalities for the Memtrace memory profiling tool. It offers
 * simplified interfaces for creating, attaching, and detaching shared
 * memory segments, as well as removing them once they are no longer needed.
 * These wrappers facilitate the management of shared memory segments used
 * by Memtrace to store memory allocation and deallocation records, enabling
 * efficient inter-process communication between the target application and
 * the profiling tool. The implementation abstracts away the complexities
 * of direct sys/shm API calls, making it easier to integrate shared memory
 * management into the broader application  architecture.
 *
 */

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
