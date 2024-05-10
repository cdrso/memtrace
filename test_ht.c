#include "ht.h"
#include <stdio.h>

#define N 5000000

int main() {
    hashTable* table = ht_create();

    for (size_t i = 0; i < N; i++) {
        size_t key = i;
        allocInfo value = {
           .block_size = i % 10 + 1,
        };
        ht_insert(table, key, value);
    }

    for (size_t i = 0; i < N; i++) {
        size_t key = i;
        ht_delete(table, key);
    }

    //ht_print_debug(table);

    /*
    for (size_t i = 0; i < N/2; i++) {
        size_t key = i;
        if (ht_get(table, key)) {
            printf("KEY: %zu NOT_DELETED\n", key);
        }
    }
    */

    ht_destroy(table);

    return 0;
}

