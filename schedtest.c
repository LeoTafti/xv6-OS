#include "types.h"
#include "user.h"
#include "sched.h"


#define NB_FORKS 4
#define OCCUPY_LEN 20000000

#define NB_CHUNKS 4
#define CHUNK_LEN (OCCUPY_LEN / NB_CHUNKS)

void occupy(int id){
    for(int i = 0; i < OCCUPY_LEN; i++){
        if(i % CHUNK_LEN == 0){ //print some feedback on progress
            printf(1, "process : %d – done %d / %d\n", id, i/CHUNK_LEN + 1, NB_CHUNKS);
        }
    }
}

int main(void){
    setscheduler(SCHED_FIFO, 0); //We don't want the parent to be preempted until it has forked all children
    int pid, i;
    for(i = 0; i < NB_FORKS; i++){
        if(fork() == 0){ //fork() returns 0 in child proc
            
            //By first setting every process to SCHED_FIFO then unsetting every other process,
            //we make sure that the first process to get to occupy() will be a FIFO one
            setscheduler(SCHED_FIFO, 0);
            if(i%2 == 1) 
              setscheduler(SCHED_RR, 0);

            occupy(i);
            exit();
        }
    }
    
    setscheduler(SCHED_RR, 0); //Now the parent can be preempted
    for(i = 0; i < NB_FORKS; i++) //Wait for each child
        wait();

    exit();
}
