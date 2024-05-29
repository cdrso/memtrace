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

void print_usage(void);
void print_ascii_art(void);

int main(int argc, char* argv[]) {
    bool h_opt = false;
    bool s_opt = false;
    bool invalid_opt = false;
    char* executable = NULL;

    int opt;
    while ((opt = getopt(argc, argv, "sh")) != -1) {
        switch (opt) {
            case 's':
                s_opt = true;
                break;
            case 'h':
                h_opt = true;
                break;
            default:
                invalid_opt = true;
                break;
        }
    }

    if (invalid_opt || h_opt || !(optind < argc)) {
        print_ascii_art();
        print_usage();
        exit(0);
    }

    hashTable* ht = ht_create();

    if (!ht) {
        printf("Could not start hashtable");
        exit(1);
    }

    pid_t pid = fork();

    if (pid == 0) {
        setenv("LD_PRELOAD", "/usr/local/lib/myalloc.so", 1);
        execvp(argv[optind], &argv[optind]);
        perror("execvp");
        exit(1);
    } else if (pid > 0) {
        int status;
        waitpid(pid, &status, 0);
        if (WIFEXITED(status)) {
            //ok, do nothing
        } else if (WIFSIGNALED(status)) {
            printf("executable process terminated due to signal %d\n", WTERMSIG(status));
        }
    } else {
        perror("fork");
        return 1;
    }

    ht_print_debug(ht, s_opt);
    ht_destroy(ht);

    return 0;
}

void print_ascii_art(void) {
    // Looks crooked but prints properly
    printf("                          _                       \n");
    printf(" _ __ ___   ___ _ __ ___ | |_ _ __ __ _  ___ ___  \n");
    printf("| '_ ` _ \\ / _ \\ '_ ` _ \\| __| '__/ _` |/ __/ _ \\ \n");
    printf("| | | | | |  __/ | | | | | |_| | | (_| | (_|  __/ \n");
    printf("|_| |_| |_|\\___|_| |_| |_|\\__|_|  \\__,_|\\___\\___| \n\n");
    printf("Copyright (C) 2024 Alejandro Cadarso\n\n");
}

void print_usage(void) {
    printf("Usage:\n");
    printf("memtrace <executable>\n");
    printf("-s option for detailed leak report including stack traces, you can investigate further ussing addr2line on these adresses.\n");
    printf("-h option to print this message\n");
}

