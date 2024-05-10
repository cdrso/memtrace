#include <stdio.h>
#include <stdlib.h>
#include <unistd.h>
#include <sys/wait.h>
#include "hashmap.h"

hashTable* ht = NULL;

int main(int argc, char* argv[]) {
    if (argc < 2) {
        fprintf(stderr, "Usage: %s <executable>\n", argv[0]);
        return 1;
    }

    ht = ht_create();
    if (!ht) {
        perror("Failure bla bla bla");
        return 1;
    }

    setenv("LD_PRELOAD", "./myalloc.so", 1);

    pid_t pid = fork(); // Fork a new process
    if (pid == 0) { // Child process
        execvp(argv[1], &argv[1]); // Replace the child process image
        perror("execvp"); // execvp will not return unless there was an error
        exit(1); // Exit the child process if execvp fails
    } else if (pid > 0) { // Parent process
        int status;
        waitpid(pid, &status, 0); // Wait for the child process to terminate
        if (WIFEXITED(status)) {
            printf("Child process exited with status %d\n", WEXITSTATUS(status));
            ht_print_debug(ht);
        } else if (WIFSIGNALED(status)) {
            ht_print_debug(ht);
            printf("Child process terminated due to signal %d\n", WTERMSIG(status));
        }
    } else {
        perror("fork");
        return 1;
    }

    ht_destroy(ht);

    return 0;
}

