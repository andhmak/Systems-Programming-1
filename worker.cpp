/* File: worker.c */

#include <stdio.h>
#include <stdlib.h>
#include <string>
#include <vector>
#include <map>
#include <fcntl.h>
#include <unistd.h>
#include <sys/stat.h>
#include <sys/types.h>
#include <signal.h>
#include <iostream>

#define PERMS 0644

#define READ 0
#define WRITE 1

int main(int argc, char* argv[]) {
    int fd;
    char buf[100];
    if ((fd = open("example", O_RDONLY)) == -1) {
        perror("open");
        exit(EXIT_FAILURE);
    }
    std::string link;
    std::map<std::string,int> link_nums;
    int nread;
    char state = 0;
    while ((nread = read(fd, buf, 100)) > 0) {
        for (int i = 0 ; i < nread ; i++) {
            if (state == 7) {
                if ((buf[i] == ' ') || (buf[i] == '\n') || (buf[i] == '/')) {
                    state = 0;
                    if (link.substr(0,4) == "www.") {
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
        perror("read");
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
    close(fd);
    
    if ((fd=open("results", O_WRONLY | O_CREAT, PERMS)) == -1) {
        perror("open");
        exit(EXIT_FAILURE);
    }
    for(std::map<std::string, int>::const_iterator it = link_nums.begin() ; it != link_nums.end() ; ++it) {
        write(fd, it->first.data(), it->first.size());
        write(fd, " ", 1);
        write(fd, std::to_string(it->second).data(), std::to_string(it->second).size());
        write(fd, "\n", 1);
    }
    close(fd);
    return (EXIT_SUCCESS);
}