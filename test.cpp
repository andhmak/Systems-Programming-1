/* File: worker.c */

#include <stdio.h>
#include <stdlib.h>
#include <string>
#include <map>
#include <fcntl.h>
#include <unistd.h>
#include <sys/stat.h>
#include <sys/types.h>
#include <signal.h>
#include <iostream>

#define PERMS 0644
#define OUTPUT_FILE "output/"

volatile sig_atomic_t sigint_received = 0;

void catchint (int signo) {
    write(1, "got signal int\n", 15);
    sigint_received = 1;
}

volatile sig_atomic_t sigterm_received = 0;

void catchterm (int signo) {
    write(1, "got signal term\n", 15);
    sigterm_received = 1;
}

int main(int argc, char* argv[]) {
    static struct sigaction act;
    act.sa_flags = 0;
    act.sa_handler = catchint;
    sigfillset(&(act.sa_mask));
    sigaction(SIGINT, &act, NULL);

    int pipe_fd;
    mkfifo("haha", 0666);
    int pid;
    if ((pid=fork()) == 0) {
        static struct sigaction act;
        act.sa_flags = 0;
        act.sa_handler = catchterm;
        sigfillset(&(act.sa_mask));
        sigaction(SIGTERM, &act, NULL);
        act.sa_handler = SIG_IGN;
        sigaction(SIGINT, &act, NULL);
        if ((pipe_fd = open("haha", O_RDONLY)) == -1) {
            perror("worker open fifo1");
            exit(EXIT_FAILURE);
        }
        char buf[100];
        fd_set fds;
        FD_ZERO(&fds);
        FD_SET(pipe_fd, &fds);
        struct timeval timeout = {0,0};
        std::string  in_file_name;
        while(!sigterm_received) {
            while(1){
                printf("wait on read");
                fflush(stdout);
                int nread = read(pipe_fd, buf, 100);
                if (nread == -1) {
                    printf("nread == -1\n");
                    fflush(stdout);
                    if (errno == EINTR) {
                        printf("errno == EINTR\n");
                        fflush(stdout);
                        if (sigterm_received) {
                            printf("sigterm_received\n");
                            fflush(stdout);
                            close(pipe_fd);
                            exit(EXIT_SUCCESS);
                        }
                        else {
                            printf("not sigterm_received\n");
                            fflush(stdout);
                            continue;
                        }
                    }
                    else {
                        printf("errno != EINTR\n");
                        fflush(stdout);
                        perror("worker read fifo");
                        close(pipe_fd);
                        exit(EXIT_FAILURE);
                    }
                }
                std::string from_pipe(buf);
                std::cout << from_pipe << std::endl;
                in_file_name.append(from_pipe.substr(0, nread));
                if (select(pipe_fd+1, &fds, (fd_set *) 0, (fd_set *) 0, &timeout) == 0) {
                    break;
                }
            }
            printf("got %s, and doing work", in_file_name.data());
            fflush(stdout);
            if (raise(SIGSTOP) != 0) {
                perror("worker raise SIGSTOP");
                close(pipe_fd);
                exit(EXIT_FAILURE);
            }
        }
        printf("finished");
        fflush(stdout);
    }
    else {
        if ((pipe_fd = open("haha", O_WRONLY)) == -1) {
            perror("worker open fifo2");
            exit(EXIT_FAILURE);
        }
        char bufg[100];
        while (!sigint_received) {
            while(read(1, bufg, 20)>0) {
                kill(pid, SIGCONT);
                printf("sending %s\n", bufg);
                fflush(stdout);
                write(pipe_fd, bufg, 20);
            }
        }
        sleep(3);
        printf("sending cont\n");
        fflush(stdout);
        kill(pid, SIGCONT);
        sleep(3);
        kill(pid, SIGTERM);
        pause();
    }
}