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
#include <signal.h>
#include <sys/errno.h>
#include <fcntl.h>
#include <unistd.h>

#define READ 0
#define WRITE 1

struct worker {
    int pid;
    std::string pipe_name;
};

volatile sig_atomic_t sigint_received = 0;
volatile sig_atomic_t sigchld_received = 0;

void catchint (int signo) {
    sigint_received = 1;
}
void catchchld (int signo) {
    sigchld_received = 1;
}

int main(int argc, char* argv[]) {
    static struct sigaction act;
    act.sa_handler = catchint;
    sigfillset(&(act.sa_mask));
    sigaction(SIGINT, &act, NULL);

    act.sa_handler = catchchld;
    sigfillset(&(act.sa_mask));
    sigaction(SIGCHLD, &act, NULL);

    std::string path = ".";
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
    if ((listen_pid = fork()) == -1) {
        perror("manager: listener fork");
        exit(EXIT_FAILURE);
    }
    /* Child */
    else if (listen_pid == 0) {
        close(listen_pipe[READ]);  // don't read from the pipe
        dup2(listen_pipe[WRITE],1); // send standard output to the pipe
        close(listen_pipe[WRITE]);  // close old descriptor for writing to the pipe
        //execl("./listener", "listener", path, NULL);    // execute the listener
        execlp("inotifywait", "inotifywait", "-m", "-e", "moved_to", "-e", "create", path.data(), NULL);   // execute inotifywait
        perror("listener execlp");
    }
    /* Parent */
    close(listen_pipe[WRITE]);  // don't write to the pipe


    fd_set fds;
    FD_ZERO(&fds);
    FD_SET(listen_pipe[READ], &fds);
    struct timeval timeout = {0,0};
    std::map<int,int> worker_to_pipe;
    std::string new_file;
    std::queue<int> available_workers;
    std::vector<int> worker_pids;
    std::vector<std::string> new_files;
    char buf[100];
    int nread;
    int num_workers;
    int worker_pid;
    int i;
    int new_pipe_fd;
    int state = 0;
    sigset_t block_set;
    sigfillset(&block_set);
    while(!sigint_received) {
        while (!sigint_received && !sigchld_received && ((nread = read(listen_pipe[READ], buf, 100)) > 0)) {
            std::cout << "manager in read loop " << nread << std::endl;
            for (i = 0 ; i < nread ; i++) {
                printf("%c", buf[i]);
                if (buf[i] == ' ') {
                    state++;
                    continue;
                }
                if (buf[i] == '\n') {
                    sigprocmask(SIG_SETMASK, &block_set, NULL);
                    new_file = path + "/" + new_file;
                    //std::cout << "manager found " + new_file<<std::endl;
                    if (available_workers.empty()) {  // if there are no available workers
                        std::string pipe_name = std::to_string(worker_pids.size());    // make named pipe
                        if (mkfifo(pipe_name.data(), 0666) < 0) {
                            perror("manager can't make fifo");
                            exit(EXIT_FAILURE);
                        }
                        if ((worker_pid = fork()) == -1) {
                            perror("manager: worker fork");
                            exit(EXIT_FAILURE);
                        }
                        else if (worker_pid == 0) {
                            //printf("creating worker\n");
                            execl("worker", "worker", pipe_name.data(), NULL);   // execute worker
                            perror("worker execlp");
                        }
                        worker_pids.push_back(worker_pid);
                        if ((new_pipe_fd = open(pipe_name.data(), O_WRONLY)) < 0) {
                            perror("manager can't open fifo");
                            exit(EXIT_FAILURE);
                        }
                        sleep(1);
                        worker_to_pipe[worker_pid] = new_pipe_fd;
                        if (write(new_pipe_fd, new_file.data(), new_file.size()) != new_file.size()) {
                            perror("manager can't write in fifo");
                            exit(EXIT_FAILURE);
                        }
                    }
                    else {
                        //here wake up worker
                    }
                    sigprocmask(SIG_UNBLOCK, &block_set, NULL);
                    //new_files.push_back(new_file);
                    new_file.erase();
                    state = 0;
                }
                else if (state == 2) {
                    new_file.push_back(buf[i]);
                }
            }
        }
    }
    printf("jjejeje");
    for (int i = 0 ; i < worker_pids.size() ; i++) {
        kill(worker_pids[i], SIGINT);
        close(worker_to_pipe[worker_pids[i]]);
        std::string pipe_name = std::to_string(i);
        if (unlink(pipe_name.data()) < 0) {
            perror("manager can't unlink fifo\n");
            exit(EXIT_FAILURE);
        }
    }
    //new_files.push_back(new_file);
    /*for(int i = 0 ; i < new_files.size();i++) {
        std::cout << new_files[i] << std::endl;
    }*/
    sleep(1);kill(listen_pid, SIGINT);   // kill listener
    /* Exiting successfully */
    exit(EXIT_SUCCESS);
}