// Simple command-line kernel monitor useful for
// controlling the kernel and exploring the system interactively.

#include "stdio.h"
#include "memlayout.h"
#include "x86.h"
#include "types.h"
#include "string.h"
#include "kdebug.h"

#include "monitor.h"

#define CMDBUF_SIZE 80 // enough for one VGA text line

struct Command {
  const char *name;
  const char *desc;
  // return -1 to force monitor to exit
  int (*func)(int argc, char **argv, struct Trapframe * tf);
};

static struct Command commands[] = {
  { "help",      "Display this list of commands",        mon_help       },
  { "info-kern", "Display information about the kernel", mon_infokern   },
  { "backtrace", "Display stack backtrace", mon_backtrace   },
};
#define NCOMMANDS (sizeof(commands)/sizeof(commands[0]))

/***** Implementations of basic kernel monitor commands *****/

int
mon_help(int argc, char **argv, struct Trapframe *tf)
{
  int i;

  for (i = 0; i < NCOMMANDS; i++)
    cprintf("%s - %s\n", commands[i].name, commands[i].desc);
  return 0;
}

int
mon_infokern(int argc, char **argv, struct Trapframe *tf)
{
  extern char _start[], entry[], etext[], edata[], end[];

  cprintf("Special kernel symbols:\n");
  cprintf("  _start                  %08x (phys)\n", _start);
  cprintf("  entry  %08x (virt)  %08x (phys)\n", entry, entry - KERNBASE);
  cprintf("  etext  %08x (virt)  %08x (phys)\n", etext, etext - KERNBASE);
  cprintf("  edata  %08x (virt)  %08x (phys)\n", edata, edata - KERNBASE);
  cprintf("  end    %08x (virt)  %08x (phys)\n", end, end - KERNBASE);
  cprintf("Kernel executable memory footprint: %dKB\n",
          ROUNDUP(end - entry, 1024) / 1024);
  return 0;
}

int
mon_backtrace(int argc, char **argv, struct Trapframe *tf)
{
  int ebp = read_ebp();
  int eip = ((int*)ebp)[1];

  while(eip >= KERNBASE){
    cprintf("ebp %08x", ebp);
    
    //eip = ebp_ptr[1];
    cprintf("  eip %08x", eip);
    
    int* ebp_ptr = (int*)ebp; //for ease of notation

    cprintf("  args");
    for(int i = 2; i <= 6; i++){
      cprintf(" %08x", ebp_ptr[i]);
    }

    struct Eipdebuginfo info = {0};
    int no_info = debuginfo_eip(eip, &info);

    if(!no_info){

      cprintf("\n       %s:%d: ", info.eip_file, info.eip_line);
      //Since info.eip_fn_name is not null terminated, we cannot print it with %s
      //we print char by char instead, using info.eip_fn_namelen
      for(int i = 0; i < info.eip_fn_namelen; i++){
        cprintf("%c", info.eip_fn_name[i]);
      }
      cprintf("+%0x", eip - info.eip_fn_addr);
    }


    cprintf("\n");

    int prev_ebp = *ebp_ptr;

    ebp = prev_ebp;
    eip = ((int*)prev_ebp)[1];

  }

  return 0;
}


/***** Kernel monitor command interpreter *****/

#define WHITESPACE "\t\r\n "
#define MAXARGS 16

static int
runcmd(char *buf, struct Trapframe *tf)
{
  int argc;
  char *argv[MAXARGS];
  int i;

  // Parse the command buffer into whitespace-separated arguments
  argc = 0;
  argv[argc] = 0;
  while (1) {
    // gobble whitespace
    while (*buf && strchr(WHITESPACE, *buf))
      *buf++ = 0;
    if (*buf == 0)
      break;

    // save and scan past next arg
    if (argc == MAXARGS-1) {
      cprintf("Too many arguments (max %d)\n", MAXARGS);
      return 0;
    }
    argv[argc++] = buf;
    while (*buf && !strchr(WHITESPACE, *buf))
      buf++;
  }
  argv[argc] = 0;

  // Lookup and invoke the command
  if (argc == 0)
    return 0;
  for (i = 0; i < NCOMMANDS; i++)
    if (strcmp(argv[0], commands[i].name) == 0)
      return commands[i].func(argc, argv, tf);
  cprintf("Unknown command '%s'\n", argv[0]);
  return 0;
}

void
monitor(struct Trapframe *tf)
{
  char *buf;

  cprintf("Welcome to the xv6 kernel monitor!\n");
  cprintf("Type 'help' for a list of commands.\n");

  while (1) {
    buf = readline("K> ");
    if (buf != NULL)
      if (runcmd(buf, tf) < 0)
        break;
  }
}
