/* File: listener.c */

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>

int main(int argc, char* argv[]) {

    //printf("Listener running, executing inotifywait on %s...\n", argv[1]);
    //fflush(stdout);
    execlp("inotifywait", "inotifywait", "-m", "-e", "moved_to", "-e", "create", argv[1], NULL);
}