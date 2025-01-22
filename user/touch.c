#include "kernel/types.h"
#include "kernel/stat.h"
#include "user/user.h"
#include "kernel/fcntl.h"

int
main(int argc, char *argv[])
{
    int fd,i;

    if(argc < 2){
        printf("Usage: touch filename\n");
        exit(1);
    }
    for(i=1; i<argc; i++){
        // Create file if it doesn't exist, or update timestamp
        fd = open(argv[i], O_RDWR | O_CREATE);
        if(fd < 0){
            printf("touch: cannot create %s\n", argv[i]);
            exit(1);
        }
        close(fd);
    }       

    exit(0);
}