#include "kernel/types.h"
#include "user.h"

int main(int argc, char *argv[]){
    int mode;

    if(argc < 3){
        printf("Usage: chmod mode file\n");
        exit(1);
    }

    // convert mode from string to integer
    mode = atoi(argv[1]);

    if(chmod(argv[2], mode) < 0){
        printf("chmod: cannot change permissions of %s\n", argv[2]);
        exit(1);
    }

    exit(0);
}