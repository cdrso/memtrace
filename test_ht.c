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

#define N 500000

int main() {
    hashTable* ht = ht_create();
    if (!ht) {
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

    for (size_t i = 1; i <= N; i++) {
        size_t key = i;
        if (ht_get(ht, key)) {
            printf("EPA\n");
        }
    }

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

