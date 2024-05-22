#include <stdlib.h>
#include <stdio.h>
#include <unistd.h> // Include unistd.h for fork()

int main(void) {
    pid_t pid;

    // Forking the first time
    pid = fork();
    if (pid == -1) { // Error occurred during fork()
        perror("Error");
        exit(EXIT_FAILURE);
    } else if (pid > 0) { // Second parent process
                          // Fork once more to create the third process
        pid = fork();
        if (pid == -1) {
            perror("Error");
            exit(EXIT_FAILURE);
        }
    }

    // At this point, we have three processes running:
    // 1. The original process (if pid == 0)
    // 2. A child process that was created by the first fork (if pid == 0 after the second fork)
    // 3. Another child process that was created by the second fork (if pid == 0 after the third fork)

    // Each process now allocates memory and frees it
    if (pid == 0) { // Child process
        void* ptr = malloc(10); // Allocate memory
        if (ptr!= NULL) {
            free(ptr); // Free the allocated memory
        } else {
            perror("Memory allocation failed");
        }
        exit(EXIT_SUCCESS);
    }

    return 0;
}
