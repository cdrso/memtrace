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
 * File: shmwrap.h
 * Author: Alejandro Cadarso
 * Date: 28-05-2024
 *
 * This header file provides the interface for managing shared memory segments
 * in C, including functions for allocating, loading, and freeing shared memory.
 * These functions facilitate the creation and management of shared memory
 * resources, essential for inter-process communication and data sharing across
 * different processes within the same system.
 *
 */

#ifndef SHMWRAP_H
#define SHMWRAP_H

#include <stdbool.h>
#include <sys/shm.h>


// Allocates space on shared memory and returns it's shmid
int shmalloc(key_t key, size_t size );

// Loads shared memory segment into current context
void* shmload(int shmid);

// Frees shared memory (needs to be loaded in current context)
bool shmfree(void* ptr, int shmid);

#endif
