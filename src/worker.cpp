/* File: worker.cpp */

#include <stdio.h>
#include <string>
#include <map>
#include <fcntl.h>
#include <unistd.h>
#include <sys/stat.h>
#include <sys/types.h>
#include <signal.h>
#include <iostream>
#include "worker_funcs.h"
#include "close_report.h"

#define OUTPUT_DIR "output/"

#define BUFFER_SIZE 100

/* Flag indicating SIGTERM was received */
volatile sig_atomic_t sigterm_received = 0;

/* The file descriptor of the worker's named pipe */
volatile sig_atomic_t pipe_fd = 0;

/* SIGTERM handler */
void catchterm (int signo) {
    //write(1, "got signal\n", 11);
    /* If the pipe has been successfully opened, stop waiting on read */
    if (pipe_fd > 0) {
        fcntl(pipe_fd, F_SETFL, fcntl(pipe_fd, F_GETFL, 0) | O_NONBLOCK);
    }
    /* Raise flag */
    sigterm_received = 1;
}

int main(int argc, char* argv[]) {
    /* Set SIGTERM handler */
    static struct sigaction act;
    act.sa_flags = 0;
    act.sa_handler = catchterm;
    sigfillset(&(act.sa_mask));
    sigaction(SIGTERM, &act, NULL);
    
    /* Ignore SIGINTs, so that the user interrupt to the manager won't possibly affect the worker */
    act.sa_handler = SIG_IGN;
    sigaction(SIGINT, &act, NULL);

    std::cout << "worker created " <<std::endl;
    fflush(stdout);

    /* Open pipe for reading without blocking, retrying if interrupted by signal */
    while (pipe_fd = open(argv[1], O_RDONLY | O_NONBLOCK)) {
        if (pipe_fd == -1) {
            if (errno != EINTR) {
                perror("worker open fifo");
                exit(EXIT_FAILURE);
            }
        }
        else {
            break;
        }
    }

    /* If successfully opened, stop and wait for the manager to wake us up */
    if (raise(SIGSTOP) != 0) {
        perror("worker raise SIGSTOP");
        close_report(pipe_fd);
        exit(EXIT_FAILURE);
    }

    /* Block on read from the pipe, only needed non-blocking opening */
    fcntl(pipe_fd, F_SETFL, fcntl(pipe_fd, F_GETFL, 0) & ~O_NONBLOCK);

    /* Put pipe into file descriptor set to call pselect() later  */
    fd_set fds;
    FD_ZERO(&fds);
    FD_SET(pipe_fd, &fds);
    struct timespec timeout = {0,0};

    /* Create block set for sigprocmask() later */
    sigset_t block_set;
    sigfillset(&block_set);

    /* Main loop, processing one file each time */
    while (1) {
        printf("worker loop\n");
        fflush(stdout);
        char buf[BUFFER_SIZE];      // buffer for reading
        std::string in_file_name;   // name of the name input file to process
        int nread;                  // number of characters read
        printf("waiting on read\n");

        /* Read the input file specified by the manager in the pipe */
        while (1) {
            /* Read at most BUFFER_SIZE bytes from the pipe */
            /* Block until there is data in the pipe or a signal interrupts */
            nread = read(pipe_fd, buf, BUFFER_SIZE);

            /* If interrupted by SIGTERM, exit successfully */
            if (sigterm_received) {
                printf("sigterm_received\n");
                fflush(stdout);
                close_report(pipe_fd);
                exit(EXIT_SUCCESS);
            }

            /* If not, but still failed to read */
            if (nread == -1) {
                /* If interrupted, retry */
                if (errno == EINTR) {
                    printf("errno == EINTR\n");
                    fflush(stdout);
                    continue;
                }
                /* Else fail */
                else {
                    perror("worker read fifo");
                    close_report(pipe_fd);
                    exit(EXIT_FAILURE);
                }
            }

            /* Append what was read to the input file name */
            in_file_name.append(buf, 0, nread);

            /* Stop reading if there is no more data in the pipe */
            if (pselect(pipe_fd+1, &fds, (fd_set *) 0, (fd_set *) 0, &timeout, &block_set) == 0) {
                break;
            }
        }

        /* Block signals and start working on the input file */
        sigprocmask(SIG_SETMASK, &block_set, NULL);

        std::cout << "worker working on " + in_file_name <<std::endl;
        std::cout << in_file_name <<std::endl;

        /* Create a map with the URLs as keys and their number of appearences in the input file as values */
        std::map<std::string,int> url_nums;
        if (extract_urls(url_nums, in_file_name.data()) == -1) {
            close_report(pipe_fd);
            exit(EXIT_FAILURE);
        }

        /* Replace the path of the input file with the output path, and add ".out" to the end, to produce the output file name */
        int slash_pos;
        for (slash_pos = in_file_name.size() - 1 ; slash_pos >= 0 ; slash_pos--) {
            if (in_file_name[slash_pos] == '/') {
                break;
            }
        }
        std::string out_file_name = OUTPUT_DIR + in_file_name.substr(slash_pos + 1, in_file_name.size() - slash_pos - 1) + ".out";

        /* Write the URLs and their appearences in the output file */
        if (write_urls(url_nums, out_file_name.data()) == -1) {
            close_report(pipe_fd);
            exit(EXIT_FAILURE);
        }

        /* Finished working on current file, unblock signals */
        sigprocmask(SIG_UNBLOCK, &block_set, NULL);

        /* Stop and wait for manager to wake us up */
        if (raise(SIGSTOP) != 0) {
            perror("worker raise SIGSTOP");
            close_report(pipe_fd);
            exit(EXIT_FAILURE);
        }
    }
}