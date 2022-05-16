/* File: close_report.cpp */

#include <errno.h>
#include <stdio.h>
#include <unistd.h>
#include "close_report.h"

/* Closes file fd point to and calls perror in case of error, retrying if interrupted */
void close_report(int fd) {
    while (close(fd) == -1) {
        if (errno != EINTR) {
            perror("close");
            break;
        }
    }
}