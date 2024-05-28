#include <stdio.h>
#include "hashmap.h"

#define NUM_ALLOCATIONS 1000

allocInfo mock = {
    .block_size = 1,
};
allocInfo mock1 = {
    .block_size = 2,
};

int main(void) {
    hashTable* ht = ht_create();

    for (int i = 1; i < NUM_ALLOCATIONS; i++) {
        if (!ht_insert(ht, i, mock)) {
        }
    }
    for (int i = 1; i < NUM_ALLOCATIONS/2; i++) {
        if(!ht_delete(ht, i)) {
            printf("wepa\n");
        }
    }
    for (int i = 1; i < NUM_ALLOCATIONS/2; i++) {
        if(ht_get(ht, i)) {
            printf("wupa\n");
        }
    }
    for (int i = NUM_ALLOCATIONS/2; i < NUM_ALLOCATIONS; i++) {
        if(!ht_get(ht, i)) {
            printf("wopa\n");
        }
    }
    for (int i = NUM_ALLOCATIONS/2; i < NUM_ALLOCATIONS; i++) {
        if(!ht_delete(ht, i)) {
            printf("wepa\n");
        }
    }
    for (int i = 1; i < NUM_ALLOCATIONS; i++) {
        if(ht_get(ht, i)) {
            printf("wapa\n");
        }
    }

    ht_insert(ht, 33, mock);
    ht_insert(ht, 33, mock1);
    ht_insert(ht, 33, mock);


    ht_print_debug(ht);
    ht_destroy(ht);

    return 0;
}
