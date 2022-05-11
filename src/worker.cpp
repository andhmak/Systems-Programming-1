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

#define PERMS 0644
#define OUTPUT_DIR "output/"

#define BUFFER_SIZE 100

volatile sig_atomic_t sigterm_received = 0;
volatile sig_atomic_t pipe_fd = 0;

/* SIGTERM handler */
void catchterm (int signo) {
    //write(1, "got signal\n", 11);
    if (pipe_fd) {
        fcntl(pipe_fd, F_SETFL, fcntl(pipe_fd, F_GETFL, 0) | O_NONBLOCK);
    }
    sigterm_received = 1;
}

int main(int argc, char* argv[]) {
    /* Set SIGTERM handler */
    static struct sigaction act;
    act.sa_flags = 0;
    act.sa_handler = catchterm;
    sigfillset(&(act.sa_mask));
    sigaction(SIGTERM, &act, NULL);
    
    /* Ignore SIGINTs, so that the user interrupt to the manager won't possibly affect */
    act.sa_handler = SIG_IGN;
    sigaction(SIGINT, &act, NULL);

    std::cout << "worker created " <<std::endl;
    fflush(stdout);

    /* Open pipe, retrying if interrupted by signal */
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
    if (raise(SIGSTOP) != 0) {
        perror("worker raise SIGSTOP");
        close(pipe_fd);
        exit(EXIT_FAILURE);
    }

    fcntl(pipe_fd, F_SETFL, fcntl(pipe_fd, F_GETFL, 0) & ~O_NONBLOCK);

    /* Put pipe into file descriptor set to call select() later  */
    fd_set fds;
    FD_ZERO(&fds);
    FD_SET(pipe_fd, &fds);
    struct timespec timeout = {0,0};

    /* Create block set for sigprocmask() later */
    sigset_t block_set;
    sigfillset(&block_set);

    /* Main loop, processing one file each time */
    while (!sigterm_received) {
        printf("worker loop\n");
        fflush(stdout);
        char buf[BUFFER_SIZE];      // buffer for reading
        std::string in_file_name;   // name of the name input file to process
        int nread;                  // number of characters read
        printf("waiting on read\n");

        /* Read the input file specified by the manager in the pipe */
        while (1) {
            nread = read(pipe_fd, buf, BUFFER_SIZE);
            if (sigterm_received) {
                printf("sigterm_received\n");
                fflush(stdout);
                close(pipe_fd);
                exit(EXIT_SUCCESS);
            }
            if (nread == -1) {
                if (errno == EINTR) {
                    printf("errno == EINTR\n");
                    fflush(stdout);
                    continue;
                }
                else {
                    perror("worker read fifo");
                    close(pipe_fd);
                    exit(EXIT_FAILURE);
                }
            }
            in_file_name.append(buf, 0, nread);
            if (pselect(pipe_fd+1, &fds, (fd_set *) 0, (fd_set *) 0, &timeout, &block_set) == 0) {
                break;
            }
        }
        sigprocmask(SIG_SETMASK, &block_set, NULL);

        std::cout << "worker working on " + in_file_name <<std::endl;
        std::cout << in_file_name <<std::endl;
        int in_fd;
        if ((in_fd = open(in_file_name.data(), O_RDONLY)) == -1) {
            perror("worker open input");
            close(pipe_fd);
            exit(EXIT_FAILURE);
        }
        std::string link;
        std::map<std::string,int> link_nums;
        char state = 0;
        while ((nread = read(in_fd, buf, BUFFER_SIZE)) > 0) {
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
        std::string out_file_name = OUTPUT_DIR + in_file_name.substr(slash_pos + 1, in_file_name.size() - slash_pos - 1) + ".out";

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

        sigprocmask(SIG_UNBLOCK, &block_set, NULL);
        if (raise(SIGSTOP) != 0) {
            perror("worker raise SIGSTOP");
            close(pipe_fd);
            exit(EXIT_FAILURE);
        }
    }
    printf("worker exiting %s, %d\n", argv[1], sigterm_received);
    close(pipe_fd);
    return (EXIT_SUCCESS);
}