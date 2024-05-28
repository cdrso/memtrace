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
 * File: memtrace.c
 * Author: Alejandro Cadarso
 * Date: 28-05-2024
 *
 * This file serves as the entry point for the Memtrace memory profiling tool.
 * It initializes a hash table for tracking memory allocations and deallocations
 * within the target application. Upon execution, it forks a child process
 * to run the target application, using a custom memory allocator library
 * (`myalloc.so`) to intercept and record memory operations. After the target
 * application exits, the parent process prints debug information about the
 * memory profile collected during its runtime.
 *
 */

#include <stdio.h>
#include <stdlib.h>
#include <unistd.h>
#include <sys/wait.h>
#include "hashtable.h"

int main(int argc, char* argv[]) {
    if (argc < 2) {
        fprintf(stderr, "Usage: %s <executable>\n", argv[0]);
        return 1;
    }

    // Looks crooked but prints properly
    printf("                          _                       \n");
    printf(" _ __ ___   ___ _ __ ___ | |_ _ __ __ _  ___ ___  \n");
    printf("| '_ ` _ \\ / _ \\ '_ ` _ \\| __| '__/ _` |/ __/ _ \\ \n");
    printf("| | | | | |  __/ | | | | | |_| | | (_| | (_|  __/ \n");
    printf("|_| |_| |_|\\___|_| |_| |_|\\__|_|  \\__,_|\\___\\___| \n\n");

    hashTable* ht = ht_create();

    if (!ht) {
        printf("Could not start hashtable");
        exit(1);
    }

    pid_t pid = fork();

    if (pid == 0) {
        setenv("LD_PRELOAD", "/usr/local/lib/myalloc.so", 1);
        execvp(argv[1], &argv[1]);
        //execvp no such file or directory, better error?
        perror("execvp");
        exit(1);
    } else if (pid > 0) {
        int status;
        waitpid(pid, &status, 0);
        if (WIFEXITED(status)) {
            ht_print_debug(ht);
        } else if (WIFSIGNALED(status)) {
            printf("executable process terminated due to signal %d\n", WTERMSIG(status));
            ht_print_debug(ht);
        }
    } else {
        perror("fork");
        return 1;
    }

    ht_destroy(ht);

    return 0;
}

