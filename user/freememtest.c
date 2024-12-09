#include "kernel/types.h"
#include "user/user.h"

int
main(int argc, char *argv[])
{
  uint64 free_mem = freemem();
  printf("Free memory: %ld bytes\n", free_mem);
  exit(0);
}
