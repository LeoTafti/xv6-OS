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

  if(f->readable == 0)
    return -1;
  if(f->type == FD_PIPE)
    return piperead(f->pipe, addr, n);
  if(f->type == FD_INODE){
    ilock(f->ip);
    if((r = readi(f->ip, addr, f->off, n)) > 0)
      f->off += r;
    iunlock(f->ip);
    return r;
  }

  panic("fileread");

}

//PAGEBREAK!
// Write to file f.
int
filewrite(struct file *f, char *addr, int n)
{
  int r;

  if(f->writable == 0)
    return -1;
  if(f->type == FD_PIPE)
    return pipewrite(f->pipe, addr, n);
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
    return i == n ? n : -1;
  }

  panic("filewrite");
}

/**
 * @brief Removes ("unregisters") a semaphore from the select "waiting" lists of file f
 * @param f the file to remove sem from
 * @param sem the semaphore of the process to unregister
 * @return 0 if successful, -1 otherwise
 */
int
fileclrsel(struct file *f, struct ksem *sem)
{
  if (f->type == FD_PIPE){
    return pipeclrsel(f->pipe, sem);
  } else if (f->type == FD_INODE){
    return clrseli(f->ip, sem);
  } else {
    return -1;
  }
}

/**
 * @brief Registers a proc's semaphore on the given select "waiting" list
 * @param list the list to register sem on
 * @param sem the semaphore of the proc to register
 * @return 0 if successful, -1 otherwise
 */
int registerproc(struct ksem* list[], struct ksem* sem){
  //Loop over list, try to find an available spot
  for(int i = 0; i<MAX_NB_SLEEPING; i++){
    if(!list[i]){
      list[i] = sem;
      return 0;
    }
  }
  return -1;
}

/**
 * @brief Atomically checks if the file is readable and if not registers the semaphore to be notified
 * @param f the file to check readable and register if needed
 * @param sem the proc's semaphore to register if needed
 * @return 1 if readable, 0 if not readable (and registered), -1 if error
 */
int fileselectread(struct file *f, struct ksem *sem){
  int readable = 0, ret = 0;
  acquire(&f->lock);

  //Check if readable
  if (f->type == FD_PIPE){
    readable = pipereadable(f->pipe);
  } else if (f->type == FD_INODE){
    readable = readablei(f->ip);
  } else {
    ret = -1;
  }

  if(!readable){
    if (f->type == FD_PIPE){
      ret = piperegister(f->pipe, sem, 1);
    } else if (f->type == FD_INODE){
      ret = registeri(f->ip, sem, 1);
    }
  }

  release(&f->lock);

  return readable ? 1 : ret;
}

/**
 * @brief Atomically checks if the file is writable and if not registers the semaphore to be notified
 * @param f the file to check writable and register if needed
 * @param sem the proc's semaphore to register if needed
 * @return 1 if writable, 0 if not writable (and registered), -1 if error
 */
int fileselectwrite(struct file *f, struct ksem *sem){
  int writable, ret = 0;
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
    if (f->type == FD_PIPE){
      ret = piperegister(f->pipe, sem, 0);
    } else if (f->type == FD_INODE){
      ret = registeri(f->ip, sem, 0);
    }
  }

  release(&f->lock);

  return writable ? 1 : ret;
}
