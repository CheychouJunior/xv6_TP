#include "kernel/types.h"
#include "kernel/stat.h"
#include "user/user.h"
#include "user/uproc.h"

int
main(void)
{
  // Header with fixed column widths
  printf("\nPID\tSTATE\t\tNAME\n");
  printf("------------------------------------\n");
  
  struct proc_info info[64];
  int count = getprocs(info);
  
  for(int i = 0; i < count; i++) {
    char *state;
    switch(info[i].state) {
      case UNUSED:    state = "unused   "; break;  // Pad with spaces
      case SLEEPING:  state = "sleep    "; break;
      case RUNNABLE:  state = "runnable "; break;
      case RUNNING:   state = "running  "; break;
      case ZOMBIE:    state = "zombie   "; break;
      default:        state = "unknown  "; break;
    }
    // Use fixed width format: 5 chars for PID, 12 for STATE, rest for NAME
    printf("%d\t%s\t%s\n", info[i].pid, state, info[i].name);
  }
  printf("\n");
  exit(0);
}