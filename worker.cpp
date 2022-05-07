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
    write(1, "got signal\n", 11);
    sigint_received = 1;
}

int main(int argc, char* argv[]) {
    static struct sigaction act;
    act.sa_handler = catchint;
    sigfillset(&(act.sa_mask));
    sigaction(SIGINT, &act, NULL);

    sigset_t block_set;
    sigfillset(&block_set);

    //act.sa_flags = SA_RESTART;
    //sigaction(SIGCONT, &act, NULL);

    std::cout << "worker created " <<std::endl;
    fflush(stdout);
    std::string pipe_name = argv[1];
    int pipe_fd;
    if ((pipe_fd = open(pipe_name.data(), O_RDONLY)) == -1) {
        perror("worker open fifo");
        exit(EXIT_FAILURE);
    }
    fd_set fds;
    FD_ZERO(&fds);
    FD_SET(pipe_fd, &fds);
    struct timeval timeout = {0,0};
    while (!sigint_received) {
        //sigprocmask(SIG_SETMASK, &block_set, NULL);
        printf("worker loop\n");
        fflush(stdout);
        char buf[100];
        std::string in_file_name;
        int nread;
        for (int i =0;i<5;i++) {
            nread = read(pipe_fd, buf, 100);
            printf("nread %d\n", nread);
            fflush(stdout);
            if (nread == 0) {
                printf("nread == 0\n");
                fflush(stdout);
                //if (errno == EINTR) {
                //printf("errno == EINTR\n");
                //fflush(stdout);
                if (sigint_received) {
                    printf("sigint_received\n");
                    fflush(stdout);
                    close(pipe_fd);
                    exit(EXIT_SUCCESS);
                }
                else {
                    printf("not sigint_received\n");
                    fflush(stdout);
                    continue;
                }
                //}
                //else {
                //    printf("errno != EINTR\n");
                //    fflush(stdout);
                //    perror("worker read fifo");
                //    close(pipe_fd);
                //    exit(EXIT_FAILURE);
                //}
            }
            std::string from_pipe(buf);
            std::cout << from_pipe << std::endl;
            in_file_name.append(from_pipe.substr(0, nread));
            if (select(pipe_fd+1, &fds, (fd_set *) 0, (fd_set *) 0, &timeout) == 0) {
                break;
            }
        }
        if (!nread) {
            printf("sfgd");
            fflush(stdout);
        }
        if (sigint_received) {
            break;
        }
        std::cout << "worker working on " + in_file_name <<std::endl;
        std::cout << in_file_name <<std::endl;
        if (sigint_received) {
            break;
        }
        int in_fd;
        if ((in_fd = open(in_file_name.data(), O_RDONLY)) == -1) {
            perror("worker open input");
            close(pipe_fd);
            exit(EXIT_FAILURE);
        }
        if (sigint_received) {
            break;
        }
        std::string link;
        std::map<std::string,int> link_nums;
        char state = 0;
        while ((nread = read(in_fd, buf, 100)) > 0) {
            for (int i = 0 ; i < nread ; i++) {
                if (state == 7) {
                    if ((buf[i] == ' ') || (buf[i] == '\n') || (buf[i] == '/')) {
                        state = 0;
                        if ((link.length() >= 4) && (link.substr(0,4) == "www.")) {
                            link.erase(0,4);
                        }
                        if (link_nums.find(link) != link_nums.end()) {
                            link_nums[link] = link_nums[link] + 1;
                        }
                        else {
                            link_nums[link] = 1;
                        }
                        link.erase();
                    }
                    else {
                        link.push_back(buf[i]);
                    }
                }
                else {
                    switch (buf[i]) {
                        case 'h':
                            state = 1;
                            break;
                        case 't':
                            if ((state == 1) || (state == 2)) {
                                state++;
                            }
                            else {
                                state = 0;
                            }
                            break;
                        case 'p':
                            state = state == 3 ? 4 : 0;
                            break;
                        case ':':
                            state = state == 4 ? 5 : 0;
                            break;
                        case '/':
                            if ((state == 5) || (state == 6)) {
                                state++;
                            }
                            else {
                                state = 0;
                            }
                            break;
                        default:
                            state = 0;
                            break;

                    }
                }
            }
        }
        if (nread == -1) {
            perror("worker read input");
            close(pipe_fd);
            close(in_fd);
            exit(EXIT_FAILURE);    
        }
        if (state == 7) {
            if (link.substr(0,4) == "www.") {
                link.erase(0,4);
            }
            if (link_nums.find(link) != link_nums.end()) {
                link_nums[link] = link_nums[link] + 1;
            }
            else {
                link_nums[link] = 1;
            }
        }
        close(in_fd);

        int slash_pos;
        for (slash_pos = in_file_name.size() - 1 ; slash_pos >= 0 ; slash_pos--) {
            if (in_file_name[slash_pos] == '/') {
                break;
            }
        }
        std::string out_file_name = OUTPUT_FILE + in_file_name.substr(slash_pos + 1, in_file_name.size() - slash_pos - 1) + ".out";

        int out_fd;
        if ((out_fd=open(out_file_name.data(), O_WRONLY | O_CREAT, PERMS)) == -1) {
            perror("worker open output");
            close(pipe_fd);
            exit(EXIT_FAILURE);
        }
        for(std::map<std::string, int>::const_iterator it = link_nums.begin() ; it != link_nums.end() ; ++it) {
            write(out_fd, it->first.data(), it->first.size());
            write(out_fd, " ", 1);
            write(out_fd, std::to_string(it->second).data(), std::to_string(it->second).size());
            write(out_fd, "\n", 1);
        }
        close(out_fd);

        //sigprocmask(SIG_UNBLOCK, &block_set, NULL);
        if (raise(SIGSTOP) != 0) {
            perror("worker raise SIGSTOP");
            close(pipe_fd);
            exit(EXIT_FAILURE);
        }
    }
    printf("worker exiting %s, %d\n", argv[1], sigint_received);
    close(pipe_fd);
    return (EXIT_SUCCESS);
}