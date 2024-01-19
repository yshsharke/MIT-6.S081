#include "kernel/types.h"
#include "kernel/stat.h"
#include "kernel/param.h"
#include "user/user.h"

int
main(int argc, char *argv[])
{
  char *p, *q;
  char *cmd;
  int pid;
  char buf[512];

  if(argc < 2){
    fprintf(2, "usage: xargs cmd [args...]\n");
    exit(1);
  }
  cmd = argv[1];

  if(read(0, buf, 512) < 0){
    fprintf(2, "xargs: read error\n");
    exit(1);
  }

  for(p = q = buf; *q; q++){
    if(*q == '\n'){
      *q = 0;
      
      pid = fork();
      if(pid == 0){
        char *t;
        char *argv_buf[MAXARG] = {0};
        int argc_exec = argc - 1;
        for(int i = 0; i < argc_exec; i++){
          argv_buf[i] = argv[i + 1];
        }

        for(t = p; t <= q; t++){
          if(*t == ' ' || *t == 0){
            *t = 0;
            if(t > p)
              argv_buf[argc_exec++] = p;
            p = t + 1;
          }
        }

        // printf("%s ", cmd);
        // for(int i = 0; i < argc_exec; i++)
        //   printf("%s ", argv_buf[i]);
        exec(cmd, argv_buf);
        fprintf(2, "xargs: exec failed\n");
        exit(1);
      } else {
        wait((int *) 0);
      }

      p = q + 1;
    }
  }
  exit(0);
}