/* File: manager.c */

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include <sys/types.h>
#include <signal.h>

#define READ 0
#define WRITE 1

int main(int argc, char* argv[]) {
    int listen_pid;
    if ((listen_pid = fork()) == -1) {
        perror("fork");
        exit(EXIT_FAILURE);
    }
    /* Child */
    else if (listen_pid == 0) {
        printf("child\n");
        fflush(stdout);
    }
    /* Parent */
    printf("parent\n");
    fflush(stdout);
    /* Exiting successfully */
    exit(EXIT_SUCCESS);
}