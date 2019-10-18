#include "types.h"
#include "user.h"
#include "sched.h"


#define NB_FORKS 6
#define OCCUPY_LEN 20000000

#define NB_CHUNKS 2
#define CHUNK_LEN (OCCUPY_LEN / NB_CHUNKS)

#define SLEEP_LEN 100

void occupy(int id){
    for(int i = 0; i < OCCUPY_LEN; i++){
        if(i % CHUNK_LEN == 0){ //print some feedback on progress
            printf(1, "process : %d – done %d / %d\n", id, i/CHUNK_LEN + 1, NB_CHUNKS);
        }
    }
}

int main(void){
    setscheduler(SCHED_FIFO, 5); //We don't want the parent to be preempted until it has forked all children
    
    int pid, i;
    for(i = 0; i < NB_FORKS; i++){
        if(fork() == 0){ //fork() returns 0 in child proc
            
            switch (i)
            {
            case 0:
                setscheduler(SCHED_FIFO, 3);
                break;
            case 1:
                setscheduler(SCHED_FIFO, 4);
                break;
            case 2:
            case 3:
                setscheduler(SCHED_RR, 3);
                break;
            case 4:
            case 5:
                setscheduler(SCHED_RR, 2);
                break;
            default:
                break;
            }

            sleep(SLEEP_LEN);

            
            //We should get there only after each child process has gone through the above (not guaranteed, but works)
            occupy(i);
            exit();
        }
    }
    
    printf(1, "Parent done creating all children\n");
    setscheduler(SCHED_RR, 0); //Now the parent can be preempted
    for(i = 0; i < NB_FORKS; i++) //Wait for each child
        wait();

    exit();
}
