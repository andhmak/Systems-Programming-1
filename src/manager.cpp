/* File: manager.cpp */

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

#define BUFFER_SIZE 50

#define PIPE_DIR "pipes/"

volatile sig_atomic_t sigint_received = 0;
volatile sig_atomic_t sigchld_received = 0;
volatile sig_atomic_t listen_read_fd = 0;
volatile sig_atomic_t async_listen_pid;

/* SIGINT handler */
void catchint (int signo) {
    //write(1, "got signal\n", 11);
    /* Stop waiting on read from listener */
    /* () */
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
    static struct sigaction act;
    act.sa_handler = catchint;
    sigfillset(&(act.sa_mask));
    sigaction(SIGINT, &act, NULL);

    act.sa_flags = SA_SIGINFO;
    act.sa_sigaction = catchchld;
    sigaction(SIGCHLD, &act, NULL);

    const char* path = ".";
	/* Initialising arguments */
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

    pid_t listen_pid;
    int listen_pipe[2];

    /* Opening pipe */
    if (pipe(listen_pipe) == -1) {
        perror("manager pipe");
        exit(EXIT_FAILURE);
    }
    listen_read_fd = listen_pipe[READ];
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
        close(listen_pipe[READ]);  // don't read from the pipe
        dup2(listen_pipe[WRITE],1); // send standard output to the pipe
        close(listen_pipe[WRITE]);  // close old descriptor for writing to the pipe
        execlp("inotifywait", "inotifywait", "-m", "-e", "moved_to", "-e", "create", path, NULL);   // execute inotifywait
        perror("listener execlp");
        exit(EXIT_FAILURE);
    }
    /* Parent */
    async_listen_pid = listen_pid;
    sigprocmask(SIG_UNBLOCK, &block_set, NULL);
    close(listen_pipe[WRITE]);  // don't write to the pipe


    std::map<int,int> worker_to_pipe;
    std::string new_file;
    std::queue<int> available_workers;
    std::vector<int> worker_pids;
    int num_workers = 0;
    int new_pipe_fd;
    int state = 0;
    std::cout << "manager starting loop" << std::endl;
    /*for (int b = 0;b<2;b++)*/
    while (!sigint_received) {
        int nread;
        std::cout << "manager in big loop "/*<<b */<< std::endl;
        char buf[BUFFER_SIZE];
        while (!sigint_received && !sigchld_received && ((nread = read(listen_pipe[READ], buf, BUFFER_SIZE)) > 0)) {
            std::cout << "manager in read loop " << nread << std::endl;
            for (uint i = 0 ; i < nread ; i++) {
                printf("%c", buf[i]);
                if (buf[i] == ' ') {
                    state++;
                    continue;
                }
                if (buf[i] == '\n') {
                    sigprocmask(SIG_SETMASK, &block_set, NULL);
                    new_file = path + ("/" + new_file);
                    std::cout << "manager found " + new_file << std::endl;
                    if (available_workers.empty()) {  // if there are no available workers
                        int worker_pid;
                        std::string pipe_name = PIPE_DIR + std::to_string(worker_pids.size());    // make named pipe
                        if (mkfifo(pipe_name.data(), 0666) < 0) {
                            perror("manager can't make fifo");
                            sigint_received = 1;
                            break;
                        }
                        if ((worker_pid = fork()) == -1) {
                            perror("manager: worker fork");
                            sigint_received = 1;
                            break;
                        }
                        else if (worker_pid == 0) {
                            sigprocmask(SIG_UNBLOCK, &block_set, NULL);
                            //printf("creating worker\n");
                            execl("./bin/worker", "worker", pipe_name.data(), NULL);   // execute worker
                            perror("worker execlp");
                            exit(EXIT_FAILURE);
                        }
                        int proc_status;
                        if (waitpid(worker_pid, &proc_status, WUNTRACED) == -1) {
                            perror("manager waitpid");
                            sigint_received = 1;
                            break;
                        }
                        if ((!WIFSTOPPED(proc_status)) || (WSTOPSIG(proc_status) != SIGSTOP)) {
                            printf("worker issue\n");
                            fflush(stdout);
                            if (unlink(pipe_name.data()) < 0) {
                                sigint_received = 1;
                                break;
                            }
                        }
                        else {
                            worker_pids.push_back(worker_pid);
                            if ((new_pipe_fd = open(pipe_name.data(), O_WRONLY)) < 0) {
                                perror("manager can't open fifo");
                                sigint_received = 1;
                                break;
                            }
                            worker_to_pipe[worker_pid] = new_pipe_fd;
                            if (write(new_pipe_fd, new_file.data(), new_file.size()) != new_file.size()) {
                                perror("manager can't write in fifo");
                                sigint_received = 1;
                                break;
                            }
                            kill(worker_pid, SIGCONT);
                            num_workers++;
                        }
                    }
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
                    sigprocmask(SIG_UNBLOCK, &block_set, NULL);
                    new_file.erase();
                    state = 0;
                }
                else if (state == 2) {
                    new_file.push_back(buf[i]);
                }
            }
        }
        if ((nread == -1) && (errno != EINTR)) {
            perror("worker read fifo");
            sigint_received = 1;
        }
        if (sigchld_received) {
            sigchld_received = 0;
            int proc_status;
            int child_pid;
            while ((child_pid = waitpid(-1, &proc_status, WNOHANG | WUNTRACED))) {
                printf("a");
                fflush(stdout);
                if (child_pid == -1) {
                    if (errno == EINTR) {
                        continue;
                    }
                    else {
                        perror("manager waitpid");
                        sigint_received = 1;
                    }
                }
                else {
                    if (WIFSTOPPED(proc_status)) {
                        if (WSTOPSIG(proc_status) == SIGSTOP) {
                            available_workers.push(child_pid);
                        }
                    }
                    else if (child_pid != listen_pid) {
                        num_workers--;
                    }
                }
            }
        }
    }
    sigprocmask(SIG_SETMASK, &block_set, NULL);
    printf("jjejeje");
    fflush(stdout);
    kill(listen_pid, SIGINT);   // kill listener
    int child_pid;
    int proc_status;
    //printf("%d, %d\n",available_workers.size(),num_workers);
    //fflush(stdout);
    while (available_workers.size() != num_workers) {
        waitpid(-1, &proc_status, WUNTRACED);
        num_workers--;
    }
    printf("a\n");
    fflush(stdout);
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
    while ((child_pid = waitpid(-1, &proc_status, 0)) > 0) {
        printf("%d", child_pid);
    }
    printf("c\n");
    fflush(stdout);
    for (int i = 0 ; i < worker_pids.size() ; i++) {
        printf("closing fifo...");
        fflush(stdout);
        close(worker_to_pipe[worker_pids[i]]);
        std::string pipe_name = "pipes/" + std::to_string(i);
        printf("unlinking fifo...");
        fflush(stdout);
        if (unlink(pipe_name.data()) < 0) {
            perror("manager can't unlink fifo\n");
            exit(EXIT_FAILURE);
        }
    }
    printf("d\n");
    fflush(stdout);
    sigprocmask(SIG_UNBLOCK, &block_set, NULL);
    printf("e\n");
    fflush(stdout);
    /* Exiting successfully */
    exit(EXIT_SUCCESS);
}