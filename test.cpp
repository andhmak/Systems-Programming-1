/* File: manager.c */

#include <stdio.h>
#include <stdlib.h>
#include <string>
#include <vector>
#include <queue>
#include <cstring>
#include <map>
#include <iostream>
#include <unistd.h>
#include <sys/types.h>
#include <sys/stat.h>
#include <sys/wait.h>
#include <signal.h>
#include <sys/errno.h>
#include <fcntl.h>
#include <unistd.h>

#define READ 0
#define WRITE 1

volatile sig_atomic_t sigint_received = 0;
volatile sig_atomic_t sigchld_received = 0;
volatile sig_atomic_t listen_read_fd = 0;
volatile sig_atomic_t async_listen_pid;

/* SIGINT handler */
void catchint (int signo) {
    //write(1, "got signal\n", 11);
    if (listen_read_fd) {
        fcntl(listen_read_fd, F_SETFL, fcntl(listen_read_fd, F_GETFL, 0) | O_NONBLOCK);
    }
    sigint_received = 1;
}

/* SIGCHLD handler */
void catchchld (int signo, siginfo_t *info, void *context) {
    /* If something happened to the listener, start the termination process */
    if (info->si_pid == async_listen_pid) {
        raise(SIGINT);
        return;
    }
    /* Else notify that a worker has finished */
    sigchld_received = 1;
}

int main(int argc, char* argv[]) {
    {std::vector<int> worker_pids;
    for (int i = 0 ; i < 5 ; i++) {
        worker_pids.push_back(i);
    }
    worker_pids.clear();}
    /* Exiting successfully */
    exit(EXIT_SUCCESS);
}