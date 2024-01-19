#include "kernel/types.h"
#include "kernel/stat.h"
#include "user/user.h"

void
sieve(int fd){
  int p;
  int pid;
  int len = 0;
  int buf[1] = {0};
  int pipe_tran[2];

  len = read(fd, buf, 1);
  if(len < 0){
    fprintf(2, "primes: read error\n");
    exit(1);
  }
  if(len == 0){
    exit(0);
  }

  p = buf[0];
  printf("prime %d\n", p);

  if(pipe(pipe_tran) < 0){
    fprintf(2, "primes: pipe failed\n");
    exit(1);
  }

  pid = fork();
  if(pid == 0){
    close(pipe_tran[1]); // unused
    sieve(pipe_tran[0]);
  } else {
    close(pipe_tran[0]); // unused

    while((len = read(fd, buf, 1)) > 0){
      int n = buf[0];
      if((n % p) != 0){
        if(write(pipe_tran[1], buf, 1) != 1){
          fprintf(2, "primes: write error\n");
          exit(1);
        }
      }
    }

    if(len == 0){
      // finish
      close(pipe_tran[1]);
      wait((int *) 0);
      exit(0);
    } else {
      fprintf(2, "primes: read error\n");
      exit(1);
    }
  }
}

int
main(int argc, char *argv[]){
  int pid;
  int pipe_gen[2];

  if(pipe(pipe_gen) < 0){
    fprintf(2, "primes: pipe failed\n");
    exit(1);
  }

  pid = fork();
  if(pid == 0){
    // sieve
    close(pipe_gen[1]); // unused
    sieve(pipe_gen[0]);
  } else {
    // generate
    close(pipe_gen[0]); // unused

    for(int i = 2; i < 35; i++){
      int buf[1] = {i};
      if(write(pipe_gen[1], buf, 1) != 1){
        fprintf(2, "primes: write error\n");
        exit(1);
      }
    }

    close(pipe_gen[1]);
    wait((int *) 0);
  }

  exit(0);
}