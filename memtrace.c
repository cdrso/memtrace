#include <stdio.h>
#include <stdlib.h>
#include <unistd.h>
#include <sys/ipc.h>
#include <sys/shm.h>
#include <sys/wait.h>
#include "hashmap.h"

int main(int argc, char* argv[]) {
    if (argc < 2) {
        fprintf(stderr, "Usage: %s <executable>\n", argv[0]);
        return 1;
    }

    hashTable* ht = ht_create();

    if (!ht) {
        printf("create returned NULL");
        exit(1);
    }

    pid_t pid = fork();

    if (pid == 0) {
        setenv("LD_PRELOAD", "./myalloc.so", 1);
        execvp(argv[1], &argv[1]);
        perror("execvp");
        exit(1);
    } else if (pid > 0) {
        int status;
        waitpid(pid, &status, 0); // Wait for the child process to terminate
        if (WIFEXITED(status)) {
            printf("Child process exited with status %d\n", WEXITSTATUS(status));
            ht_load_context(ht);
            ht_print_debug(ht);
        } else if (WIFSIGNALED(status)) {
            printf("Child process terminated due to signal %d\n", WTERMSIG(status));
            ht_load_context(ht);
            ht_print_debug(ht);
        }
    } else {
        perror("fork");
        ht_load_context(ht);
        ht_destroy(ht);
        return 1;
    }

    ht_destroy(ht);

    return 0;
}

