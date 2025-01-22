#include "kernel/types.h"
#include "kernel/stat.h"
#include "user/user.h"

#define CLEAR_SCREEN "\033[2J"    // Code ANSI pour effacer l'écran
#define CURSOR_HOME "\033[H"      // Code ANSI pour replacer le curseur en haut à gauche

int main(int argc, char *argv[])
{
    // Écriture des séquences d'échappement ANSI sur la sortie standard
    write(1, CLEAR_SCREEN, sizeof(CLEAR_SCREEN) - 1);
    write(1, CURSOR_HOME, sizeof(CURSOR_HOME) - 1);
    exit(0);
}
