#include "kernel/types.h"
#include "kernel/stat.h"
#include "user/user.h"
#include "kernel/fcntl.h"

#define BUFFER_SIZE 512

int
main(int argc, char *argv[])
{
  int fd_source, fd_dest;
  int n;
  char buffer[BUFFER_SIZE];
  
  if(argc != 3) {
    fprintf(2, "Usage: cp source_file destination_file\n");
    exit(1);
  }
  
  // Open source file for reading
  if((fd_source = open(argv[1], O_RDONLY)) < 0) {
    fprintf(2, "cp: cannot open %s\n", argv[1]);
    exit(1);
  }
  
  // Create/open destination file for writing
  if((fd_dest = open(argv[2], O_CREATE | O_WRONLY)) < 0) {
    fprintf(2, "cp: cannot create %s\n", argv[2]);
    close(fd_source);
    exit(1);
  }
  
  // Copy data in chunks
  while((n = read(fd_source, buffer, sizeof(buffer))) > 0) {
    if(write(fd_dest, buffer, n) != n) {
      fprintf(2, "cp: write error\n");
      close(fd_source);
      close(fd_dest);
      exit(1);
    }
  }
  
  if(n < 0) {
    fprintf(2, "cp: read error\n");
    close(fd_source);
    close(fd_dest);
    exit(1);
  }
  
  close(fd_source);
  close(fd_dest);
  exit(0);
}