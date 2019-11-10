#include "types.h"
#include "defs.h"
#include "param.h"
#include "mmu.h"
#include "proc.h"
#include "fs.h"
#include "file.h"
#include "spinlock.h"
#include "ksem.h"

#define PIPESIZE 512

struct pipe {
  struct spinlock lock;
  char data[PIPESIZE];
  uint nread;     // number of bytes read
  uint nwrite;    // number of bytes written
  int readopen;   // read fd is still open
  int writeopen;  // write fd is still open
  struct ksem* selreadable[MAX_NB_SLEEPING]; //List of (semaphores' of) processes waiting for the pipe to become readable
  struct ksem* selwritable[MAX_NB_SLEEPING]; //List of (semaphores' of) processes waiting for the pipe to become writable
};

int
pipealloc(struct file **f0, struct file **f1)
{
  struct pipe *p;

  p = 0;
  *f0 = *f1 = 0;
  if((*f0 = filealloc()) == 0 || (*f1 = filealloc()) == 0)
    goto bad;
  if((p = (struct pipe*)kalloc()) == 0)
    goto bad;
  p->readopen = 1;
  p->writeopen = 1;
  p->nwrite = 0;
  p->nread = 0;
  
  initlock(&p->lock, "pipe");
  (*f0)->type = FD_PIPE;
  (*f0)->readable = 1;
  (*f0)->writable = 0;
  (*f0)->pipe = p;
  (*f1)->type = FD_PIPE;
  (*f1)->readable = 0;
  (*f1)->writable = 1;
  (*f1)->pipe = p;

  memset(p->selreadable, 0, sizeof(p->selreadable));
  memset(p->selwritable, 0, sizeof(p->selwritable));

  return 0;

//PAGEBREAK: 20
 bad:
  if(p)
    kfree((char*)p);
  if(*f0)
    fileclose(*f0);
  if(*f1)
    fileclose(*f1);
  return -1;
}

void
pipeclose(struct pipe *p, int writable)
{
  acquire(&p->lock);
  if(writable){
    p->writeopen = 0;
    // Wake up anything waiting to read
    for(int i = 0; i < MAX_NB_SLEEPING; i++){
      if(p->selreadable[i]){
        ksem_up(p->selreadable[i]);
      }
    }
    
    wakeup(&p->nread);
  } else {
    p->readopen = 0;
    // Wake up anything waiting to write
    for(int i = 0; i < MAX_NB_SLEEPING; i++){
      if(p->selwritable[i]){
        ksem_up(p->selwritable[i]);
      }
    }

    
    wakeup(&p->nwrite);
  }
  if(p->readopen == 0 && p->writeopen == 0){
    release(&p->lock);
    kfree((char*)p);
  } else
    release(&p->lock);
}

//PAGEBREAK: 40
int
pipewrite(struct pipe *p, char *addr, int n)
{
  int i;

  acquire(&p->lock);
  for(i = 0; i < n; i++){
    while(p->nwrite == p->nread + PIPESIZE) {  //DOC: pipewrite-full
      if(p->readopen == 0 || proc->killed){
        release(&p->lock);
        return -1;
      }
      wakeup(&p->nread);
      sleep(&p->nwrite, &p->lock);  //DOC: pipewrite-sleep
    }
    p->data[p->nwrite++ % PIPESIZE] = addr[i];
  }
  
  // Wake up anything waiting to read
  for(int i = 0; i < MAX_NB_SLEEPING; i++){
    if(p->selreadable[i]){
      ksem_up(p->selreadable[i]);
    }
  }
  
  wakeup(&p->nread);  //DOC: pipewrite-wakeup1
  release(&p->lock);
  return n;
}

int
piperead(struct pipe *p, char *addr, int n)
{
  int i;

  acquire(&p->lock);
  while(p->nread == p->nwrite && p->writeopen){  //DOC: pipe-empty
    if(proc->killed){
      release(&p->lock);
      return -1;
    }
    sleep(&p->nread, &p->lock); //DOC: piperead-sleep
  }
  for(i = 0; i < n; i++){  //DOC: piperead-copy
    if(p->nread == p->nwrite)
      break;
    addr[i] = p->data[p->nread++ % PIPESIZE];
  }
  
  // Wake up anything waiting to write
  for(int i = 0; i < MAX_NB_SLEEPING; i++){
    if(p->selwritable[i]){
      ksem_up(p->selwritable[i]);
    }
  }
  
  wakeup(&p->nwrite);  //DOC: piperead-wakeup
  release(&p->lock);
  return i;
}

/**
 * TODO doc
 */
int
pipeclrsel(struct pipe *p, struct ksem *sem) {
  int ret = 0;

  acquire(&p->lock);
  for(int i = 0; i < MAX_NB_SLEEPING; i++){
    if(p->selreadable[i] == sem){
      p->selreadable[i] = (void*)0;
      ret = 1;
      break;
    }
    if(p->selwritable[i] == sem){
      p->selwritable[i] = (void*)0;
      ret = 1;
      break;
    }
  }
  release(&p->lock);

  return ret;
}

/**
 * TODO : doc
 */
int
piperegister(struct pipe *p, struct ksem *sem, int on_read_list){
  int ret;
  acquire(&p->lock);

  if(on_read_list)
    ret = registerproc(p->selreadable, sem);
  else
    ret = registerproc(p->selwritable, sem);

  release(&p->lock);

  return ret;
}

/**
 * TODO : doc
 * @return -1 if error, 0 if not readable, 1 if readable
 */
int
pipereadable(struct pipe *p) {
  if(proc->killed)
    return -1;

  return (p->nwrite > p->nread || !p->writeopen) ? 1:0;
}

/**
 * TODO : doc
 * @return -1 if error, 0 if not readable, 1 if readable
 */
int
pipewritable(struct pipe *p){
  if(!p->readopen || proc->killed)
      return -1;

  return (p->nwrite - p->nread != PIPESIZE) ? 1:0;
}

