/* File: worker_funcs.cpp */

#include <fcntl.h>
#include <unistd.h>
#include "worker_funcs.h"
#include "close_report.h"

/* Function that cleans url and adds it to url_nums */
void add_url(std::map<std::string,int> &url_nums, std::string &url) {
    /* Remove possible "www." */
    if ((url.length() >= 4) && (url.substr(0,4) == "www.")) {
        url.erase(0,4);
    }
    /* If we've already found the same URL, add an extra occurence */
    if (url_nums.find(url) != url_nums.end()) {
        url_nums[url] = url_nums[url] + 1;
    }
    /* Else set the number of occurences to 1 */
    else {
        url_nums[url] = 1;
    }
}

/* Function that finds the number of appearences of each URL in file_name and puts the result in url_nums */
/* Returns 0 on success, -1 on failure */
int extract_urls(std::map<std::string,int> &url_nums, const char* file_name) {
    /* Open the file */
    int fd;
    if ((fd = open(file_name, O_RDONLY)) == -1) {
        perror("worker: open input file");
        return -1;
    }

    /* Process the file */
    std::string url;        // current URL read
    char buf[BUFFER_SIZE];  // input buffer
    int nread;              // bytes read
    char state = 0;         // current state
    /* Read file in BUFFER_SIZE chuncks */
    while ((nread = read(fd, buf, BUFFER_SIZE)) > 0) {
        /* For each chunk in memory, process it character by character */
        for (int i = 0 ; i < nread ; i++) {
            /* If we're inside the location of a URL */
            if (state == 7) {
                /* If we've reached the end of the location */
                if ((buf[i] == ' ') || (buf[i] == '\n') || (buf[i] == '/')) {
                    /* Reset the state */
                    state = 0;
                    /* Add the URL to the map */
                    add_url(url_nums, url);
                    /* Erase the URL */
                    url.erase();
                }
                /* Else add the characted to the current URL */
                else {
                    url.push_back(buf[i]);
                }
                /* Check if there is a slash at the end and we need to ignore the rest until a whitespace */
                if (buf[i] == '/') {
                    state = 8;
                }
            }
            /* If we're past the slash in the URL */
            else if (state == 8) {
                /* Ignore until the URL terminates */
                if ((buf[i] == ' ') || (buf[i] == '\n')) {
                    state = 0;
                }
            }
            /* Change states to recognise the "http://" sequence */
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
    /* Return error if file couldn't be read */
    if (nread == -1) {
        perror("worker: read input file");
        close_report(fd);
        return -1;   
    }
    /* Check for possible URL at the end of the file with no whitespace terminating it */
    if (state == 7) {
        add_url(url_nums, url);
    }

    /* Close the file */
    close_report(fd);

    /* Return successfully */
    return 0;
}

/* Function that outputs the number of appearences of each URL in url_nums to file_name */
/* Returns 0 on success, -1 on failure */
int write_urls(const std::map<std::string,int> &url_nums, const char* file_name) {
    /* Open the file */
    int fd;
    if ((fd=open(file_name, O_WRONLY | O_CREAT, PERMS)) == -1) {
        perror("worker: open output");
        return -1;
    }

    /* Write the output to the file */
    for(std::map<std::string, int>::const_iterator it = url_nums.begin() ; it != url_nums.end() ; ++it) {
        std::string line = it->first + " " + std::to_string(it->second) + "\n";
        if (write(fd, line.data(), line.size()) < line.size()) {
            perror("worker: write output");
            close_report(fd);
            return -1;
        }
    }

    /* Close the file */
    close_report(fd);

    /* Return successfully */
    return 0;
}