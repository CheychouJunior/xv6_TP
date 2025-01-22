#include "kernel/types.h"
#include "kernel/stat.h"
#include "user/user.h"
#include "kernel/fs.h"

#define MAX_PATH 128

char*
get_current_dir()
{
  static char path[MAX_PATH];
  int fd;
  struct dirent de;
  struct stat st, prev_st;
  char buf[MAX_PATH];

  // Commencer au répertoire courant
  strcpy(path, ".");
  
  // Ouvrir le répertoire courant
  if((fd = open(".", 0)) < 0){
    fprintf(2, "pwd: cannot open current directory\n");
    exit(1);
  }

  // Obtenir les informations sur le répertoire courant
  if(fstat(fd, &st) < 0){
    fprintf(2, "pwd: cannot stat current directory\n");
    close(fd);
    exit(1);
  }

  prev_st = st;  // Sauvegarder l'état du répertoire courant

  // Boucler jusqu'à atteindre le répertoire racine
  while(st.ino != 1){
    close(fd);
    
    // Aller au répertoire parent
    if(chdir("..") < 0){
      fprintf(2, "pwd: cannot cd to parent\n");
      exit(1);
    }

    // Chercher l'entrée correspondant au répertoire précédent
    if((fd = open(".", 0)) < 0){
      fprintf(2, "pwd: cannot open directory\n");
      exit(1);
    }

    while(read(fd, &de, sizeof(de)) == sizeof(de)){
      if(de.inum == 0)
        continue;
      
      if(stat(de.name, &st) < 0)
        continue;
      
      if(st.ino == prev_st.ino){  // Comparer avec l'état précédent
        // Construire le chemin
        memmove(buf, path, strlen(path) + 1);
        strcpy(path, "/");
        strcpy(path + 1, de.name);
        strcpy(path + strlen(path), buf);
        break;
      }
    }
    
    if(fstat(fd, &prev_st) < 0){  // Mettre à jour l'état précédent
      fprintf(2, "pwd: cannot stat directory\n");
      close(fd);
      exit(1);
    }
  }

  close(fd);

  // Si le chemin est vide, c'est la racine
  if(strlen(path) == 0)
    strcpy(path, "/");

  return path;
}

int
main(int argc, char *argv[])
{
  char *path = get_current_dir();
  printf("%s\n", path);
  exit(0);
}