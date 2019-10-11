#include "types.h"
#include "user.h"
#include "sched.h"


#define NB_FORKS 4
#define OCCUPY_LEN 20000000

#define NB_CHUNKS 10
#define CHUNK_LEN (OCCUPY_LEN / NB_CHUNKS)

void occupy(int id){
    for(int i = 0; i < OCCUPY_LEN; i++){
        if(i % CHUNK_LEN == 0){ //print some feedback on progress
            printf(1, "process : %d – done %d / %d\n", id, i/CHUNK_LEN + 1, NB_CHUNKS);
        }
    }
}

int main(void){
    int pid, i;
    for(i = 0; i < NB_FORKS; i++){
        if(fork() == 0){ //fork() returns 0 in child proc
            if(i%2 == 1) //Change the policy from SCHED_RR (default) to SCHED_FIFO for every other child process
              setscheduler(SCHED_FIFO);
            occupy(i);
            exit();
        }
    }

    for(i = 0; i < NB_FORKS; i++) //Wait for each child
        wait();

    exit();
}
