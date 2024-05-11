#include <stdio.h>
#include <stdlib.h>
#include <unistd.h>
#include <sys/ipc.h>
#include <sys/shm.h>
#include <sys/wait.h>
#include "hashmap.h"

#define HASH_TABLE_SIZE sizeof(hashTable)
#define HASH_TABLE_ENTRY_SIZE sizeof(hashTableEntry)
#define HASH_TABLE_INIT_ENTRIES 103

int main(int argc, char* argv[]) {
    if (argc < 2) {
        fprintf(stderr, "Usage: %s <executable>\n", argv[0]);
        return 1;
    }

    key_t shkey_ht = ftok("/tmp", 'A');

    int shmid_ht = shmget(shkey_ht, HASH_TABLE_SIZE, IPC_CREAT | 0666);
     if (shmid_ht < 0) {
        perror("shmget");
        exit(1);
    }
    char shmid_ht_str[256];
    sprintf(shmid_ht_str, "%d", shmid_ht);
    setenv("HT_SHMID", shmid_ht_str, 1);

    key_t shkey_entries = ftok("/tmp", 'B');

    int shmid_entries = shmget(shkey_entries, HASH_TABLE_ENTRY_SIZE * HASH_TABLE_INIT_ENTRIES, IPC_CREAT | 0666);
     if (shmid_entries < 0) {
        perror("shmget");
        exit(1);
    }
    char shmid_entries_str[256];
    sprintf(shmid_entries_str, "%d", shmid_entries);
    setenv("HT_ENTRIES_SHMID", shmid_entries_str, 1);

    hashTable* ht = (hashTable*)shmat(shmid_ht, NULL, 0);
    if (ht == (void*)-1) {
        perror("shmat");
        exit(1);
    }

    hashTableEntry* ht_entries = (hashTableEntry*)shmat(shmid_entries, NULL, 0);
    if (ht_entries == (void*)-1) {
        perror("shmat");
        exit(1);
    }

    if (!ht_create(ht, ht_entries)) {
        printf("create returned NULL");
        exit(1);
    }


    pid_t pid = fork();

    if (pid == 0) {
        char* env[] = {"LD_PRELOAD=./myalloc.so"};
        execve(argv[1], &argv[1], env);
        perror("execvp");
        exit(1);
    } else if (pid > 0) {
        int status;
        waitpid(pid, &status, 0); // Wait for the child process to terminate
        ht->entries = shmat(shmid_entries, NULL, 0);
        if (WIFEXITED(status)) {
            printf("Child process exited with status %d\n", WEXITSTATUS(status));
            ht_print_debug(ht);
        } else if (WIFSIGNALED(status)) {
            printf("Child process terminated due to signal %d\n", WTERMSIG(status));
            ht_print_debug(ht);
        }
    } else {
        perror("fork");
        return 1;
    }

    ht_destroy(ht);

    return 0;
}

