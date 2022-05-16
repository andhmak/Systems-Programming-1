/* File: close_report.h */

/* Closes file fd point to and calls perror in case of error, retrying if interrupted */
void close_report(int fd);