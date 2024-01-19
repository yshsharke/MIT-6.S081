#include "kernel/types.h"
#include "kernel/stat.h"
#include "user/user.h"

int
main(int argc, char *argv[])
{
  int pid;
  int pipe1[2];
  int pipe2[2];
  char buf[1] = {0};

  if(pipe(pipe1) < 0 || pipe(pipe2) < 0){
    fprintf(2, "pingpong: pipe failed\n");
    exit(1);
  }

  pid = fork();
  if(pid == 0){
    // child
    // read from pipe1[0], write to pipe2[1]
    pid = getpid();

    close(pipe2[0]), close(pipe1[1]); // unused

    if(read(pipe1[0], buf, 1) < 0){
      fprintf(2, "pingpong: read error\n");
      exit(1);
    }

    printf("%d: received ping\n", pid);

    if(write(pipe2[1], buf, 1) != 1){
      fprintf(2, "pingpong: write error\n");
      exit(1);
    }

    close(pipe1[0]), close(pipe2[1]);
  } else {
    // parent
    // read from pipe2[0], write to pipe1[1]
    pid = getpid();

    close(pipe1[0]), close(pipe2[1]); // unused

    if(write(pipe1[1], buf, 1) != 1){
      fprintf(2, "pingpong: write error\n");
      exit(1);
    }

    if(read(pipe2[0], buf, 1) < 0){
      fprintf(2, "pingpong: read error\n");
      exit(1);
    }

    printf("%d: received pong\n", pid);

    close(pipe2[0]), close(pipe1[1]);
  }

  exit(0);
}