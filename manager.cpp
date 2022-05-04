/* File: manager.c */

#include <stdio.h>
#include <stdlib.h>
#include <string>
#include <vector>
#include <cstring>
#include <iostream>
#include <unistd.h>
#include <sys/types.h>
#include <signal.h>

#define READ 0
#define WRITE 1

int main(int argc, char* argv[]) {

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
        perror("pipe");
        exit(EXIT_FAILURE);
    }
    if ((listen_pid = fork()) == -1) {
        perror("fork");
        exit(EXIT_FAILURE);
    }
    /* Child */
    else if (listen_pid == 0) {
        close(listen_pipe[READ]);  // don't read from the pipe
        dup2(listen_pipe[WRITE],1); // send standard output to the pipe
        close(listen_pipe[WRITE]);  // close old descriptor for writing to the pipe
        //execl("./listener", "listener", path, NULL);    // execute the listener
        execlp("inotifywait", "inotifywait", "-m", "-e", "moved_to", "-e", "create", path.data(), NULL);   // execute inotifywait
        perror("execlp");
    }
    /* Parent */
    close(listen_pipe[WRITE]);  // don't write to the pipe

    std::vector<std::string> new_files;
    std::string new_file;
    //std::map<std::string,int> link_nums;
    char buf[100];
    int nread;
    int i;
    while ((nread = read(listen_pipe[READ], buf, 100)) > 0) {
        for (i = 0 ; i < nread ; i++) {
            if (buf[i] == '\n') {
                new_files.push_back(new_file);
            }
            else {
                new_file.push_back(buf[i]);
            }
        }
    }
    new_files.push_back(new_file);
    for(int i = 0 ; i < new_files.size();i++) {
        std::cout << new_files[i] << std::endl;
    }
    sleep(1);kill(listen_pid, SIGINT);   // kill listener
    /* Exiting successfully */
    exit(EXIT_SUCCESS);
}