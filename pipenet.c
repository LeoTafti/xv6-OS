#include "types.h"
#include "user.h"
#include "stat.h"
#include "syscall.h"
#include "select.h"

#define BUFSIZE 100

char * shargv[] = { "sh", 0 };

void pipenet(void) {
  int toshfds[2], fromshfds[2];
  char buf1[BUFSIZE];
  char buf2[BUFSIZE];
  int pid;

  //fd_set readfds, writefds;

  if (pipe(toshfds) != 0) {
    printf(2, "Pipe failed!");
    exit();
  }
  if (pipe(fromshfds)) {
    printf(2, "Pipe failed!");
    exit();
  }

  // if child
  if ((pid = fork()) == 0) {
    close(toshfds[1]);   // close write end
    close(fromshfds[0]); // close read end
    close(0);            // close console stdin
    dup(toshfds[0]);     // dup pipe to stdin
    close(1);            // close console stdout
    close(2);            // close console stderr
    dup(fromshfds[1]);   // dup pipe to stdout
    dup(fromshfds[1]);   // dup pipe to stderr
    close(toshfds[0]);   // close duplicated read fd
    close(fromshfds[1]); // close duplicated write fd

    exec(shargv[0], shargv); // invoke shell
  } else {
    close(toshfds[0]);  // close read end
    close(fromshfds[1]);// close write end

    fd_set readfds, writefds;
    int bytesbuffered1 = 0, bytesbuffered2 = 0;
    
    //Compute nfds value
    int maxfd = fromshfds[0] > toshfds[1] ? fromshfds[0] : toshfds[1];
    int nfds = maxfd + 1;

    printf(1,"\n\n ***** Starting PipeNet ***** \n\n");
    for (;;) {

      //Initialize
      FD_ZERO(&readfds);
      FD_ZERO(&writefds);

      //Set readfds
      FD_SET(0, &readfds);
      FD_SET(fromshfds[0], &readfds);
      
      //Set writefds
      FD_SET(toshfds[1], &writefds);
      FD_SET(1, &writefds);

      if(select(nfds, &readfds, &writefds) < 0){
        printf(2, "Select failed\n");
        exit();
      }


      if(FD_ISSET(0, &readfds) && bytesbuffered1 < BUFSIZE){
        //printf(1, "\nReading from console\n");
        int bytesread = read(0, buf1 + bytesbuffered1, sizeof(buf1) - bytesbuffered1); // read from console
        if(bytesread < 0){
          printf(2, "Read error (1)");
          exit();
        }
        bytesbuffered1 += bytesread;
      }

      if(FD_ISSET(toshfds[1], &writefds) && bytesbuffered1 > 0){ //bytesbuffered > 0 checks that we have something to write in the buffer
        //printf(1, "\nWriting to pipe\n");
        int byteswritten = write(toshfds[1], buf1, bytesbuffered1); // write console data to pipe
        if(byteswritten < 0){
          printf(2, "Write error (1)");
          exit();
        }

        //Move remaining bytes (if any) to the beggining of the buffer (no circular buffer for simplicity)
        memmove(buf1, buf1+byteswritten, bytesbuffered1 - byteswritten);

        //Update bytesbuffered
        bytesbuffered1 -= byteswritten;
      }

      if(FD_ISSET(fromshfds[0], &readfds) && bytesbuffered2 < BUFSIZE){
          //printf(1, "\nReading from pipe\n");
          int bytesread = read(fromshfds[0], buf2 + bytesbuffered2, sizeof(buf2) - bytesbuffered2); // read from pipe
          if(bytesread < 0){
            printf(2, "Read error (2)");
            exit();
          }
          bytesbuffered2 += bytesread;
      }

      if(FD_ISSET(1, &writefds) && bytesbuffered2 > 0){
        int byteswritten = write(1, buf2, bytesbuffered2);
        if(byteswritten < 0){
          printf(2, "Write error (2)");
          exit();
        }

        memmove(buf2, buf2+byteswritten, bytesbuffered2 - byteswritten);

        bytesbuffered2 -= byteswritten;
      }
    }
    wait();
  }
}

int main(void) {
  pipenet();
  exit();
}
