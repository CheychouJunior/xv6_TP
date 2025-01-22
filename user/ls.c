#include "kernel/types.h"
#include "kernel/stat.h"
#include "user/user.h"
#include "kernel/fs.h"
#include "kernel/fcntl.h"

char*
fmtname(char *path)
{
  static char buf[DIRSIZ+1];
  char *p;

  // Find first character after last slash.
  for(p=path+strlen(path); p >= path && *p != '/'; p--)
    ;
  p++;

  // Return blank-padded name.
  if(strlen(p) >= DIRSIZ)
    return p;
  memmove(buf, p, strlen(p));
  memset(buf+strlen(p), ' ', DIRSIZ-strlen(p));
  return buf;
}

char*
ret_type(int type)
{
  if(type == 2)
    return "-";
  if(type == 1)
    return "d";
  if(type == 3)
    return "b";
  return "?";
}

char*
ret_perm(int perm)
{
  switch(perm){
    case 0: return "---";
            break;
    case 1: return "--x";
            break;
    case 2: return "-w-";
            break;
    case 3: return "-wx";
            break;
    case 4: return "r--";
            break;
    case 5: return "r-x";
            break;
    case 6: return "rw-";
            break;
    case 7: return "rwx";
            break;
    default: return "---";
  }
}

void
ls(char *path)
{
  char buf[512], *p;
  int fd;
  struct dirent de;
  struct stat st;

  if((fd = open(path, O_RDONLY)) < 0){
    fprintf(2, "ls: cannot open %s\n", path);
    return;
  }

  if(fstat(fd, &st) < 0){
    fprintf(2, "ls: cannot stat %s\n", path);
    close(fd);
    return;
  }

  switch(st.type){
  case T_DEVICE:
  case T_FILE:
    printf("%s%s %s %d %d\n", ret_type(st.type), ret_perm(st.mode), fmtname(path), st.ino, (int) st.size);
    break;

  case T_DIR:
    if(strlen(path) + 1 + DIRSIZ + 1 > sizeof buf){
      printf("ls: path too long\n");
      break;
    }
    strcpy(buf, path);
    p = buf+strlen(buf);
    *p++ = '/';
    while(read(fd, &de, sizeof(de)) == sizeof(de)){
      if(de.inum == 0)
        continue;
      memmove(p, de.name, DIRSIZ);
      p[DIRSIZ] = 0;
      if(stat(buf, &st) < 0){
        printf("ls: cannot stat %s\n", buf);
        continue;
      }
      printf("%s%s %s %d %d\n", ret_type(st.type), ret_perm(st.mode), fmtname(buf), st.ino, (int) st.size);
    }
    break;
  }
  close(fd);
}

int
main(int argc, char *argv[])
{
  int i;

  if(argc < 2){
    ls(".");
    exit(0);
  }

  for(i=1; i<argc; i++)
    ls(argv[i]);
  exit(0);
}
