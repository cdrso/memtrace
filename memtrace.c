#include <stdio.h>
#include <stdlib.h>
#include <pthread.h>
#include <string.h>
#include <unistd.h>
#include "hashmap.h"

hashTable* ht = NULL;

void* execute_child(void* arg) {
    setenv("LD_PRELOAD", "./myalloc.so", 1);
    execlp(*(char**)arg, *(char**)arg, NULL);
    perror("execlp");
    exit(1);
}

int main(int argc, char* argv[]) {
    if (argc < 2) {
        fprintf(stderr, "Usage: %s <executable>\n", argv[0]);
        return 1;
    }

    //No intercepts here
    ht_create(&ht);
    if (!ht) {
        printf("create returned NULL");
        exit(1);
    }

    pthread_t tid;
    if (pthread_create(&tid, NULL, execute_child, (void*)&argv[1])) {
        perror("pthread_create");
        ht_destroy(ht);
        return 1;
    }

    void* result;
    if (pthread_join(tid, &result)) {
        perror("pthread_join");
        ht_destroy(ht);
        return 1;
    }

    ht_print_debug(ht);

    ht_destroy(ht);

    return 0;
}
