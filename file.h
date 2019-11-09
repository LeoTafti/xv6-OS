#include "select.h"
#include "spinlock.h"

#define MAX_NB_SLEEPING 16

struct file {
  enum { FD_NONE, FD_PIPE, FD_INODE } type;
  int ref; // reference count
  char readable;
  char writable;
  struct pipe *pipe;
  struct inode *ip;
  uint off;
  struct spinlock lock;
  struct ksem* selreadable[MAX_NB_SLEEPING];
  struct ksem* selwritable[MAX_NB_SLEEPING];
};

int registerproc(struct ksem* list[], struct ksem* sem);

// in-memory copy of an inode
struct inode {
  uint dev;           // Device number
  uint inum;          // Inode number
  int ref;            // Reference count
  int flags;          // I_BUSY, I_VALID

  short type;         // copy of disk inode
  short major;
  short minor;
  short nlink;
  uint size;
  uint addrs[NDIRECT+1];
};
#define I_BUSY 0x1
#define I_VALID 0x2

// table mapping major device number to
// device functions
struct devsw {
  int (*read)(struct inode*, char*, int);
  int (*write)(struct inode*, char*, int);
  //int (*clrsel)(struct inode*, struct ksem *sem); //TODO : remove if I don't use it
  int (*readable)(struct inode*);
  int (*writable)(struct inode*);
};

extern struct devsw devsw[];

#define CONSOLE 1

//PAGEBREAK!
// Blank page.
