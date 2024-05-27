#include <stdio.h>
#include <stdlib.h>
#include <unistd.h>
#include <sys/wait.h>
#include "hashmap.h"

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

