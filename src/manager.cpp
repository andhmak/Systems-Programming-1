/* File: manager.cpp */

#include <stdio.h>
#include <unistd.h>
#include <sys/types.h>
#include <sys/stat.h>
#include <sys/wait.h>
#include <signal.h>
#include <sys/errno.h>
#include <fcntl.h>
#include <string>
#include <cstring>
#include <vector>
#include <queue>
#include <map>
#include <iostream>
#include "manager_funcs.h"
#include "close_report.h"

#define READ 0
#define WRITE 1

#define BUFFER_SIZE 50

#define PIPE_DIR "pipes/"

/* Flag indicating SIGINT was received */
volatile sig_atomic_t sigint_received = 0;

/* Flag indicating SIGCHLD was received */
volatile sig_atomic_t sigchld_received = 0;

/* The listener's pid */
volatile sig_atomic_t async_listen_pid;

/* The file descriptor of the listener pipe */
volatile sig_atomic_t listen_read_fd = 0;

/* SIGINT handler */
void catchint (int signo) {
    //write(1, "got signal\n", 11);
    /* Stop waiting on read from listener */
    /* (in case signal comes after the flag is evaluated and before the read()) */
    if (listen_read_fd) {
        fcntl(listen_read_fd, F_SETFL, fcntl(listen_read_fd, F_GETFL, 0) | O_NONBLOCK);
    }
    /* Set the flag */
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
    /* Set signal handlers */
    static struct sigaction act;
    act.sa_handler = catchint;
    sigfillset(&(act.sa_mask));
    sigaction(SIGINT, &act, NULL);

    act.sa_flags = SA_SIGINFO;
    act.sa_sigaction = catchchld;
    sigaction(SIGCHLD, &act, NULL);

	/* Initialising arguments */
    const char* path = ".";
    if ((argc != 1) && (argc != 3)) {
        fprintf(stderr, "Invalid number of arguments\n");
        exit(EXIT_FAILURE);
    }
    if (argc == 3) {
        if (strcmp(argv[1], "-p")) {
            fprintf(stderr, "sniffer: invalid option -- '%s'\n", argv[1]);
            exit(EXIT_FAILURE);
        }
        path = argv[2];
    }

    /* Opening pipe to be used with listener */
    pid_t listen_pid;
    int listen_pipe[2];

    if (pipe(listen_pipe) == -1) {
        perror("manager pipe");
        exit(EXIT_FAILURE);
    }
    listen_read_fd = listen_pipe[READ]; // passing the fd to global atomic variable so that the handler can make it non-blocking if needed

    /* Create listener */

    /* Block signals so that everything can be safely set up */
    sigset_t block_set;
    sigfillset(&block_set);
    sigprocmask(SIG_SETMASK, &block_set, NULL);
    if ((listen_pid = fork()) == -1) {
        perror("manager: listener fork");
        exit(EXIT_FAILURE);
    }

    /* Child */
    else if (listen_pid == 0) {
        sigprocmask(SIG_UNBLOCK, &block_set, NULL);
        close_report(listen_pipe[READ]);    // don't read from the pipe
        dup2(listen_pipe[WRITE],1);         // send standard output to the pipe
        close_report(listen_pipe[WRITE]);   // close old descriptor for writing to the pipe
        execlp("inotifywait", "inotifywait", "-m", "-e", "moved_to", "-e", "create", path, NULL);   // execute inotifywait
        perror("listener execlp");
        exit(EXIT_FAILURE);
    }

    /* Parent */
    async_listen_pid = listen_pid;              // pass the listener's pid to atomic global variable so that a handler can access it
    sigprocmask(SIG_UNBLOCK, &block_set, NULL); // and after that's done we can safely unblock the signals

    close_report(listen_pipe[WRITE]);           // don't write to the pipe

    /* Main loop */
    std::queue<int> available_workers;  // queue of available workers
    std::vector<int> worker_pids;       // vector of pids of all workers successfully created
    int num_alive_workers = 0;          // number of workers still alive
    std::map<int,int> worker_to_pipe;   // map from worker's pid to the fd of the equivalent named pipe
    std::string new_file;               // new file reported by the listener
    int state = 0;                      // current state while processing the data read from the listener
    std::cout << "manager starting loop" << std::endl;
    while (!sigint_received) {
        char buf[BUFFER_SIZE];  // input buffer
        int nread;              // number of bytes read
        std::cout << "manager in big loop " << std::endl;
        /* If no signal has been received, read from the listener, and block until there is data to read */
        while (!sigint_received && !sigchld_received && ((nread = read(listen_pipe[READ], buf, BUFFER_SIZE)) > 0)) {
            std::cout << "manager in read loop " << nread << std::endl;
            /* After reading a chunk, process it character by character in memory */
            for (uint i = 0 ; i < nread ; i++) {
                printf("%c", buf[i]);
                /* Increment state when reading spaces */
                if (buf[i] == ' ') {
                    state++;
                    continue;
                }
                /* After the second space, we start reading the file name */
                if (buf[i] != '\n') {
                    if (state == 2) {
                        new_file.push_back(buf[i]);
                    }
                }
                /* When reaching a newline, we have read a complete file name, and we send it to a worker for processing */
                else {
                    state = 0;
                    /* Block signals in the critical region */
                    sigprocmask(SIG_SETMASK, &block_set, NULL);
                    /* Add the path to the file name */
                    new_file = path + ("/" + new_file);
                    std::cout << "manager found " + new_file << std::endl;
                    /* If there are no available workers */
                    if (available_workers.empty()) {
                        /* Assign the job to a new worker */
                        if (make_worker(new_file, PIPE_DIR, worker_pids, worker_to_pipe, num_alive_workers, block_set) == -1) {
                            sigint_received = 1;
                            break;
                        }
                    }
                    /* Else wake up an available worker */
                    else {
                        int available_worker = available_workers.front();
                        available_workers.pop();
                        if (write(worker_to_pipe[available_worker], new_file.data(), new_file.size()) != new_file.size()) {
                            perror("manager can't write in fifo");
                            sigint_received = 1;
                            break;
                        }
                        kill(available_worker, SIGCONT);
                    }
                    /* Exiting critical region */
                    sigprocmask(SIG_UNBLOCK, &block_set, NULL);
                    /* Clearing file name */
                    new_file.erase();
                }
            }
        }
        /* If read failed without being interrupted, end the program */
        if ((nread == -1) && (errno != EINTR)) {
            perror("worker read fifo");
            sigint_received = 1;
            break;
        }
        /* If a child changed state */
        if (sigchld_received) {
            sigchld_received = 0;
            /* Update the relevant structures */
            if (gather_children(available_workers, num_alive_workers, listen_pid) == -1) {
                sigint_received = 1;
            }
        }
    }

    /* Ending the program */
    /* Block all signals, we don't need to handle any of them while terminating */
    sigprocmask(SIG_SETMASK, &block_set, NULL);
    printf("jjejeje");
    fflush(stdout);
    /* Kill the listener */
    kill(listen_pid, SIGINT);
    //printf("%d, %d\n",available_workers.size(),num_workers);
    //fflush(stdout);
    /* Wait for all workers to finish working */
    while (available_workers.size() != num_alive_workers) {
        waitpid(-1, NULL, WUNTRACED);
        num_alive_workers--;
    }
    printf("a\n");
    fflush(stdout);

    /* Terminate all workers */
    for (int i = 0 ; i < worker_pids.size() ; i++) {
        printf("continuing worker...");
        fflush(stdout);
        kill(worker_pids[i], SIGCONT);
        printf("interrupting worker...");
        fflush(stdout);
        kill(worker_pids[i], SIGTERM);
    }
    printf("b\n");
    fflush(stdout);

    /* Wait for all workers to terminate */
    while (waitpid(-1, NULL, 0) > 0);
    printf("c\n");
    fflush(stdout);

    /* Close and then unlink all named pipes */
    for (int i = 0 ; i < worker_pids.size() ; i++) {
        printf("closing fifo...");
        fflush(stdout);
        close_report(worker_to_pipe[worker_pids[i]]);
        std::string pipe_name = "pipes/" + std::to_string(i);
        printf("unlinking fifo...");
        fflush(stdout);
        if (unlink(pipe_name.data()) < 0) {
            perror("manager can't unlink fifo\n");
            exit(EXIT_FAILURE);
        }
    }
    
    /* Exiting successfully */
    exit(EXIT_SUCCESS);
}