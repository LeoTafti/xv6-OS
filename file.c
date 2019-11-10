//
// File descriptors
//

#include "types.h"
#include "defs.h"
#include "param.h"
#include "fs.h"
#include "file.h"
#include "spinlock.h"
#include "proc.h"
#include "ksem.h"

struct devsw devsw[NDEV];
struct {
  struct spinlock lock;
  struct file file[NFILE];
} ftable;

void
fileinit(void)
{
  initlock(&ftable.lock, "ftable");
}

// Allocate a file structure.
struct file*
filealloc(void)
{
  struct file *f;

  acquire(&ftable.lock);
  for(f = ftable.file; f < ftable.file + NFILE; f++){
    if(f->ref == 0){
      f->ref = 1;
      release(&ftable.lock);
      return f;
    }
  }
  release(&ftable.lock);
  return 0;
}

// Increment ref count for file f.
struct file*
filedup(struct file *f)
{
  acquire(&ftable.lock);
  if(f->ref < 1)
    panic("filedup");
  f->ref++;
  release(&ftable.lock);
  return f;
}

// Close file f.  (Decrement ref count, close when reaches 0.)
void
fileclose(struct file *f)
{
  struct file ff;

  acquire(&ftable.lock);
  if(f->ref < 1)
    panic("fileclose");
  if(--f->ref > 0){
    release(&ftable.lock);
    return;
  }
  ff = *f;
  f->ref = 0;
  f->type = FD_NONE;
  release(&ftable.lock);

  if(ff.type == FD_PIPE)
    pipeclose(ff.pipe, ff.writable);
  else if(ff.type == FD_INODE){
    begin_op();
    iput(ff.ip);
    end_op();
  }
}

// Get metadata about file f.
int
filestat(struct file *f, struct stat *st)
{
  if(f->type == FD_INODE){
    ilock(f->ip);
    stati(f->ip, st);
    iunlock(f->ip);
    return 0;
  }
  return -1;
}

// Read from file f.
int
fileread(struct file *f, char *addr, int n)
{
  int r;
  int ret = -1;

  if(f->readable == 0)
    return -1;
  if(f->type == FD_PIPE)
    ret = piperead(f->pipe, addr, n);
  if(f->type == FD_INODE){
    ilock(f->ip);
    if((r = readi(f->ip, addr, f->off, n)) > 0)
      f->off += r;
    iunlock(f->ip);
    ret = r;
  }
  
  if(ret > 0){
    //Read successful – wake up processes waiting to write
    acquire(&f->lock);
    for(int i = 0; i < MAX_NB_SLEEPING; i++){
      if(f->selwritable[i])
        ksem_up(f->selwritable[i]);
    }
    release(&f->lock);
  }

  return ret;
  //panic("fileread"); // TODO : remove ?

}

//PAGEBREAK!
// Write to file f.
int
filewrite(struct file *f, char *addr, int n)
{
  int r;
  int ret = -1;

  if(f->writable == 0)
    return -1;
  if(f->type == FD_PIPE)
    ret = pipewrite(f->pipe, addr, n);
  if(f->type == FD_INODE){
    // write a few blocks at a time to avoid exceeding
    // the maximum log transaction size, including
    // i-node, indirect block, allocation blocks,
    // and 2 blocks of slop for non-aligned writes.
    // this really belongs lower down, since writei()
    // might be writing a device like the console.
    int max = ((LOGSIZE-1-1-2) / 2) * 512;
    int i = 0;
    while(i < n){
      int n1 = n - i;
      if(n1 > max)
        n1 = max;

      begin_op();
      ilock(f->ip);
      if ((r = writei(f->ip, addr + i, f->off, n1)) > 0)
        f->off += r;
      iunlock(f->ip);
      end_op();

      if(r < 0)
        break;
      if(r != n1)
        panic("short filewrite");
      i += r;
    }
    ret = i == n ? n : -1;
  }

  if (ret > 0){
    //Write successful – wake up processes waiting to read
    acquire(&f->lock);
    for(int i = 0; i < MAX_NB_SLEEPING; i++){
      if(f->type == FD_PIPE) cprintf("considering f %x\n", f);
      if(f->selreadable[i]){
        if(f->type == FD_PIPE) cprintf("really waking up\n");
        ksem_up(f->selreadable[i]);
      }
    }
    release(&f->lock);
  }

  return ret;
  //panic("filewrite"); //TODO : remove?
}

/**
 * TODO : doc
 */
int
fileclrsel(struct file *f, struct ksem *sem)
{
  int cleared = 0;
  acquire(&f->lock);
  for(int i = 0; i < MAX_NB_SLEEPING; i++){
    if(f->selreadable[i] == sem){
      f->selreadable[i] = (void*)0;
      cleared = 1;
    }
    if(f->selwritable[i] == sem){
      f->selwritable[i] = (void*)0;
      cleared = 1;
    }
  }
  release(&f->lock);
  //TODO : remove if unused
  // if (f->type == FD_PIPE){
  //   return pipeclrsel(f->pipe, sem);
  // } else if (f->type == FD_INODE){
  //   return clrseli(f->ip, sem);
  // } else {
  //   return -1;
  // }

  return cleared ? 0 : -1;
}

/**
 * TODO : doc
 */
int registerproc(struct ksem* list[], struct ksem* sem){
  //Loop over list, try to find an available spost
  for(int i = 0; i<MAX_NB_SLEEPING; i++){
    if(!list[i]){
      list[i] = sem;
      return 0;
    }
  }
  return -1;
}

/**
 * TODO doc
 */
int fileselectread(struct file *f, struct ksem *sem){
  int readable, ret;
  acquire(&f->lock);

  //Check if readable
  if (f->type == FD_PIPE){
    readable = pipereadable(f->pipe);
  } else if (f->type == FD_INODE){
    readable = readablei(f->ip);
  } else {
    return -1;
  }
 
  if(!readable){
    cprintf("Registering for f %x\n", f);
    ret = registerproc(f->selreadable, sem);
    cprintf("Registering ret val %d", ret);
  }

  release(&f->lock);

  return readable ? 1 : ret;
}

/**
 * TODO doc
 */
int fileselectwrite(struct file *f, struct ksem *sem){
  int writable, ret;
  acquire(&f->lock);

  //Check if writable
  if (f->type == FD_PIPE){
    writable = pipewritable(f->pipe);
  } else if (f->type == FD_INODE){
    writable = writablei(f->ip);
  } else {
    return -1;
  }
 
  if(!writable){
    ret = registerproc(f->selwritable, sem);
  }

  release(&f->lock);

  return writable ? 1 : ret;
}
