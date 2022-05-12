/* File: manager_funcs.cpp */

#include <unistd.h>
#include <signal.h>
#include <sys/wait.h>
#include <fcntl.h>
#include <sys/stat.h>
#include "manager_funcs.h"
#include "close_report.h"

/* Attempts to create a worker and give it new_file to process, updating all structures as necessary. */
/* Returns 0 on success, -1 on failure. If manager is fine but worker failed, it is considered a success. */
int make_worker(std::string &new_file, const char* pipe_dir, std::vector<int> &worker_pids, std::map<int,int> &worker_to_pipe, int &num_alive_workers, sigset_t block_set) {
    /* Make named pipe */
    std::string pipe_name = pipe_dir + std::to_string(worker_pids.size());
    if (mkfifo(pipe_name.data(), 0666) < 0) {
        perror("manager: can't make fifo");
        return -1;
    }

    /* Fork worker */
    int worker_pid;
    if ((worker_pid = fork()) == -1) {
        perror("manager: worker fork");
        return -1;
    }

    /* Child */
    else if (worker_pid == 0) {
        /* Unblock signals */
        sigprocmask(SIG_UNBLOCK, &block_set, NULL);
        //printf("creating worker\n");
        /* Execute worker */
        execl("./bin/worker", "worker", pipe_name.data(), NULL);
        perror("worker: execl");
        exit(EXIT_FAILURE);
    }

    /* Parent */
    /* Wait for worker to either stop (success) or terminate (failure) */
    int proc_status;
    if (waitpid(worker_pid, &proc_status, WUNTRACED) == -1) {
        perror("manager: waitpid");
        return -1;
    }
    /* If it terminated, move on */
    if ((!WIFSTOPPED(proc_status)) || (WSTOPSIG(proc_status) != SIGSTOP)) {
        printf("worker issue\n");
        fflush(stdout);
        /* Destroy the pipe made */
        if (unlink(pipe_name.data()) < 0) {
            return -1;
        }
    }
    /* If it succeeded, pass it the information about the file it needs to process and update the structures */
    else {
        /* Add pid to workersand increase number of alive workers by 1 */
        worker_pids.push_back(worker_pid);
        num_alive_workers++;

        /* Open pipe for writing */
        int new_pipe_fd;
        if ((new_pipe_fd = open(pipe_name.data(), O_WRONLY)) < 0) {
            perror("manager: can't open fifo");
            return -1;
        }

        /* Associate the pipe's fd to the worker's pid */
        worker_to_pipe[worker_pid] = new_pipe_fd;

        /* Write the file name to the pipe */
        if (write(new_pipe_fd, new_file.data(), new_file.size()) != new_file.size()) {
            perror("manager: can't write in fifo");
            return -1;
        }
        
        /* Continue the worker */
        kill(worker_pid, SIGCONT);
    }

    /* Exit successfully */
    return 0;
}

/* Wait without blocking for all children that have changed state, updating the queue of available workers and the number of alive workers. */
/* Returns 0 on success, -1 on failure. */
int gather_children(std::queue<int> &available_workers, int &num_alive_workers, int listen_pid) {
    /* Wait while there are children with changed states */
    int child_pid, proc_status;
    while ((child_pid = waitpid(-1, &proc_status, WNOHANG | WUNTRACED))) {
        printf("a");
        fflush(stdout);
        /* If waitpid failed */
        if (child_pid == -1) {
            /* Retry if interrupted */
            if (errno == EINTR) {
                continue;
            }
            /* Else fail */
            else {
                perror("manager: waitpid");
                return -1;
            }
        }
        /* If a worker changed state */
        else if (child_pid != listen_pid) {
            /* If it stopped */
            if (WIFSTOPPED(proc_status)) {
                if (WSTOPSIG(proc_status) == SIGSTOP) {
                    /* Add it to the queue */
                    available_workers.push(child_pid);
                }
            }
            /* Else if it was terminated decrement the number of alive workers */
            else {
                num_alive_workers--;
            }
        }
    }

    /* Exit successfully */
    return 0;
}