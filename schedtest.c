#include "types.h"
#include "user.h"


#define NB_CHILDREN 2
#define OCCUPY_LEN 10000
#define NB_CHUNKS 10
#define CHUNK_LEN (OCCUPY_LEN / NB_CHUNKS)

void occupy(int pid){
    for(int i = 0; i < OCCUPY_LEN; i++){
        if(i % CHUNK_LEN == 0){
            printf(1, "pid : %d – done %d / %d", pid, i/CHUNK_LEN, NB_CHUNKS);
        }
    }
}

int main(void){
    int pid;
    for(int i = 0; i < NB_CHILDREN; i++){
        if((pid = fork()) != -1){
            occupy(pid);
        }
    }
}