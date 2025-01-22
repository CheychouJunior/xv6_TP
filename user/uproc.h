#ifndef _UPROC_H_
#define _UPROC_H_

// Only include what's needed for the user program
enum procstate { UNUSED, SLEEPING, RUNNABLE, RUNNING, ZOMBIE };

struct proc_info {
  int pid;
  enum procstate state;
  char name[16];
};

#endif