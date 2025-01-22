#include "kernel/types.h"
#include "kernel/stat.h"
#include "user/user.h"

int
main(int argc, char *argv[])
{
  // Test 1: Répertoire racine
  if(chdir("/") < 0){
    printf("pwdtest: cd / failed\n");
    exit(1);
  }
  printf("Test 1 - Root directory:\n");
  if(fork() == 0){
    exec("pwd", argv);
  }
  wait(0);

  // Test 2: Créer et naviguer dans un nouveau répertoire
  mkdir("/testdir");
  if(chdir("/testdir") < 0){
    printf("pwdtest: cd /testdir failed\n");
    exit(1);
  }
  printf("\nTest 2 - New directory:\n");
  if(fork() == 0){
    exec("pwd", argv);
  }
  wait(0);

  exit(0);
}