#include "kernel/types.h"
#include "user/user.h"

int main() {
    // Créer un mutex
    int mid = mutex_create();
    if(mid < 0) {
        printf("Erreur création mutex\n");
        exit(1);
    }

    // Créer deux processus
    if(fork() == 0) {
        // Processus fils
        mutex_lock(mid);
        printf("Processus fils a le verrou\n");
        sleep(100);  // Simuler un travail
        printf("Processus fils libère le verrou\n");
        mutex_unlock(mid);
        exit(0);
    } else {
        // Processus parent
        mutex_lock(mid);
        printf("Processus parent a le verrou\n");
        sleep(100);  // Simuler un travail
        printf("Processus parent libère le verrou\n");
        mutex_unlock(mid);
        wait(0);
    }

    // Libérer le mutex
    mutex_free(mid);
    exit(0);
}
