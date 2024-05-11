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

#define N 1000

int main() {
    key_t shkey_ht = ftok("/tmp", 'A');
    int shmid_ht = shmget(shkey_ht, HASH_TABLE_SIZE, IPC_CREAT | 0666);
     if (shmid_ht < 0) {
        perror("shmget");
        exit(1);
    }

    key_t shkey_entries = ftok("/tmp", 'B');
    int shmid_entries = shmget(shkey_entries, HASH_TABLE_ENTRY_SIZE * HASH_TABLE_INIT_ENTRIES, IPC_CREAT | 0666);
     if (shmid_entries < 0) {
        perror("shmget");
        exit(1);
    }

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
    char shmid_entries_str[256];
    sprintf(shmid_entries_str, "%d", shmid_entries);
    setenv("HT_ENTRIES_SHMID", shmid_entries_str, 1);

    if (!ht_create(ht, ht_entries)) {
        printf("create returned NULL");
        exit(1);
    }

    ht_print_debug(ht);

    for (size_t i = 1; i <= N; i++) {
        size_t key = i;
        allocInfo value = {
           .block_size = i % 10 + 1,
        };
        ht_insert(ht, key, value);
    }
    ht_print_debug(ht);

    for (size_t i = 1; i <= N; i++) {
        size_t key = i;
        if (!ht_get(ht, key)) {
            printf("EPA\n");
        }
    }

    for (size_t i = 1; i <= N; i++) {
        size_t key = i;
        ht_delete(ht, key);
    }


    printf("should be deleted\n");
    ht_print_debug(ht);

    /*
    for (size_t i = 0; i < N/2; i++) {
        size_t key = i;
        if (ht_get(table, key)) {
            printf("KEY: %zu NOT_DELETED\n", key);
        }
    }
    */

    ht_destroy(ht);

    return 0;
}

