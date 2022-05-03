/* File: manager.c */

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include <sys/types.h>
#include <signal.h>

#define READ 0
#define WRITE 1

int main(int argc, char* argv[]) {
    pid_t  pid;
  int fd[2], bytes;

  if (pipe(fd) == -1){ perror("pipe"); exit(1); }
  if ( (pid = fork()) == -1 ){ perror("fork"); exit(1); }

  if ( pid != 0 ){    // parent and writer
     close(fd[READ]);
     dup2(fd[WRITE],1);
     close(fd[WRITE]);
     execlp(argv[1], argv[1], NULL);
     perror("execlp");
     }
  else{                // child
     close(fd[WRITE]);
     dup2(fd[READ],0);
     close(fd[READ]);
     char buf[12];
     scanf("%s", buf);
     printf("%s", buf);
     }
}