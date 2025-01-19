#ifndef _MUTEX_H_
#define _MUTEX_H_

struct mutex {
  uint locked;       // 0 = déverrouillé, 1 = verrouillé
  int pid;          // PID du processus détenant le verrou
  struct spinlock lk;  // spinlock pour protéger la structure mutex
};

#define MAX_MUTEXES 64  // Nombre maximum de mutex dans le système
#endif
