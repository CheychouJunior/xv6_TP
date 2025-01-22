// kernel/mutex.c - Implémentation des opérations mutex
#include "types.h"
#include "param.h"
#include "memlayout.h"
#include "riscv.h"
#include "spinlock.h"
#include "proc.h"
#include "defs.h"
#include "mutex.h"

struct {
  struct mutex mutexes[MAX_MUTEXES];
  struct spinlock lock;
} mtable;

void
mutex_init(void)
{
  initlock(&mtable.lock, "mutexes");
  for(int i = 0; i < MAX_MUTEXES; i++) {
    initlock(&mtable.mutexes[i].lk, "mutex");
    mtable.mutexes[i].locked = 0;
    mtable.mutexes[i].pid = 0;
  }
}

// Alloue un nouveau mutex
int
mutex_create(void)
{
  acquire(&mtable.lock);
  for(int i = 0; i < MAX_MUTEXES; i++) {
    if(mtable.mutexes[i].pid == 0) {
      mtable.mutexes[i].pid = myproc()->pid;
      release(&mtable.lock);
      return i;
    }
  }
  release(&mtable.lock);
  return -1;
}

// Verrouille un mutex
int
mutex_lock(int mutex_id)
{
  if(mutex_id < 0 || mutex_id >= MAX_MUTEXES)
    return -1;
    
  struct mutex *m = &mtable.mutexes[mutex_id];
  acquire(&m->lk);
  
  while(m->locked) {
    sleep(m, &m->lk);
  }
  
  m->locked = 1;
  m->pid = myproc()->pid;
  release(&m->lk);
  return 0;
}

// Déverrouille un mutex
int
mutex_unlock(int mutex_id)
{
  if(mutex_id < 0 || mutex_id >= MAX_MUTEXES)
    return -1;
    
  struct mutex *m = &mtable.mutexes[mutex_id];
  acquire(&m->lk);
  
  if(m->pid != myproc()->pid) {
    release(&m->lk);
    return -1;
  }
  
  m->locked = 0;
  m->pid = 0;
  wakeup(m);
  release(&m->lk);
  return 0;
}

// Libère un mutex
int
mutex_free(int mutex_id)
{
  if(mutex_id < 0 || mutex_id >= MAX_MUTEXES)
    return -1;
    
  struct mutex *m = &mtable.mutexes[mutex_id];
  acquire(&m->lk);
  
  if(m->pid != myproc()->pid) {
    release(&m->lk);
    return -1;
  }
  
  m->locked = 0;
  m->pid = 0;
  wakeup(m);
  release(&m->lk);
  return 0;
}
