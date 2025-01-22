#include "kernel/types.h"
#include "kernel/stat.h"
#include "user/user.h"
#include "kernel/fs.h"
#include "kernel/fcntl.h"

#define MAXLEN 100
#define MAXPATH 256
#define MAXCONTENT 1024
#define HISTORY_SIZE 10
#define TRASH_PATH "/.trash"
#define CLEAR_SCREEN "\033[2J"    // Code ANSI pour effacer l'écran
#define CURSOR_HOME "\033[H"      // Code ANSI pour replacer le curseur en haut à gauche

char back_history[HISTORY_SIZE][MAXPATH];
char forward_history[HISTORY_SIZE][MAXPATH];
int back_index = 0, forward_index = 0;
int history_active = 0; // Indique si on est en train de naviguer dans l'historique


struct Clipboard {
    int mode;           // 0: vide, 1: copie, 2: couper
    char path[MAXPATH]; // Chemin source complet
    char name[MAXLEN];  // Nom de l'élément
    int isDir;          // 0: fichier, 1: répertoire
    int is_persistent;  // 1: garder après navigation, 0: effacer après coller
};

struct Action {
    int type;  // 1: suppression, 2: changement de répertoire
    char path[MAXPATH];
    char oldname[MAXLEN];
    char newname[MAXLEN];
};

// Après les déclarations de structures et avec les autres variables globales

struct Action history[HISTORY_SIZE];
int history_index = 0;
char current_path[MAXPATH];
char previous_path[MAXPATH];
struct Clipboard clipboard = {0, "", "", 0};  // Ajout de cette ligne

struct archive_header {
    char name[MAXLEN];
    uint size;
};


// Fonctions utilitaires
void clear_screen() {
    write(1, CLEAR_SCREEN, sizeof(CLEAR_SCREEN) - 1);
    write(1, CURSOR_HOME, sizeof(CURSOR_HOME) - 1);
}

void append_path(char *dest, char *src) {
    while(*dest) dest++;
    while((*dest++ = *src++));
}

void copy_file(char* src, char* dst) {
    char buf[512];
    int fd_src, fd_dst, n;

    fd_src = open(src, O_RDONLY);
    if(fd_src < 0) {
        printf("Erreur: Impossible d'ouvrir le fichier source\n");
        return;
    }

    fd_dst = open(dst, O_CREATE | O_WRONLY);
    if(fd_dst < 0) {
        printf("Erreur: Impossible de créer le fichier destination\n");
        close(fd_src);
        return;
    }

    while((n = read(fd_src, buf, sizeof(buf))) > 0) {
        if(write(fd_dst, buf, n) != n) {
            printf("Erreur d'écriture\n");
            close(fd_src);
            close(fd_dst);
            unlink(dst);
            return;
        }
    }

    close(fd_src);
    close(fd_dst);
}


void rename_file(char *oldname, char *newname) {
    struct stat st;

    // Vérifier si le fichier source existe
    if (stat(oldname, &st) < 0) {
        printf("Erreur: Fichier source introuvable\n");
        return;
    }

    // Essayer de renommer le fichier directement
    if (link(oldname, newname) < 0) {
        printf("Erreur: Impossible de renommer le fichier\n");
        return;
    }

    // Supprimer l'ancien fichier après la copie réussie
    if (unlink(oldname) < 0) {
        printf("Attention: L'ancien fichier n'a pas pu être supprimé\n");
        return;
    }

    printf("Fichier renommé avec succès de '%s' à '%s'\n", oldname, newname);
}


void rename_directory(char *oldname, char *newname) {
    struct stat st;

    // Vérifier si le répertoire source existe
    if (stat(oldname, &st) < 0 || st.type != T_DIR) {
        printf("Erreur: Répertoire source introuvable ou non valide\n");
        return;
    }

    // Créer le nouveau répertoire
    if (mkdir(newname) < 0) {
        printf("Erreur: Impossible de créer le nouveau répertoire '%s'\n", newname);
        return;
    }

    int fd;
    struct dirent de;

    fd = open(oldname, 0);
    if (fd < 0) {
        printf("Erreur: Impossible d'ouvrir le répertoire source '%s'\n", oldname);
        unlink(newname); // Nettoyage
        return;
    }

    // Parcourir et copier le contenu du répertoire
    while (read(fd, &de, sizeof(de)) == sizeof(de)) {
        if (de.inum == 0 || strcmp(de.name, ".") == 0 || strcmp(de.name, "..") == 0)
            continue;

        char old_path[MAXPATH], new_path[MAXPATH];
        strcpy(old_path, oldname);
        append_path(old_path, "/");
        append_path(old_path, de.name);

        strcpy(new_path, newname);
        append_path(new_path, "/");
        append_path(new_path, de.name);

        if (stat(old_path, &st) < 0)
            continue;

        if (st.type == T_DIR) {
            rename_directory(old_path, new_path);
        } else {
            copy_file(old_path, new_path);
            unlink(old_path);
        }
    }

    close(fd);

    // Supprimer le répertoire source
    if (unlink(oldname) < 0) {
        printf("Erreur: Impossible de supprimer le répertoire source '%s'\n", oldname);
        return;
    }

    printf("Répertoire renommé avec succès de '%s' à '%s'\n", oldname, newname);
}




// Fonctions de gestion de la corbeille
void init_trash() {
    mkdir(TRASH_PATH);
}

void move_to_trash(char* filename) {
    char trash_path[MAXPATH];
    struct stat st;
    
    strcpy(trash_path, TRASH_PATH);
    append_path(trash_path, "/");
    append_path(trash_path, filename);
    
    if(stat(filename, &st) < 0) {
        printf("Erreur: Fichier introuvable\n");
        return;
    }
    
    if(st.type == T_DIR) {
        rename_directory(filename, trash_path);
    } else {
        copy_file(filename, trash_path);
        unlink(filename);
    }
    printf("Déplacé vers la corbeille: %s\n", filename);
}

void empty_trash() {
    int fd;
    struct dirent de;
    struct stat st;
    char path[MAXPATH];
    
    fd = open(TRASH_PATH, 0);
    if(fd < 0) {
        printf("Impossible d'ouvrir la corbeille\n");
        return;
    }
    
    while(read(fd, &de, sizeof(de)) == sizeof(de)) {
        if(de.inum == 0 || strcmp(de.name, ".") == 0 || strcmp(de.name, "..") == 0)
            continue;
            
        strcpy(path, TRASH_PATH);
        append_path(path, "/");
        append_path(path, de.name);
        
        if(stat(path, &st) < 0)
            continue;
            
        if(st.type == T_DIR) {
            int subfd;
            struct dirent subde;
            char subpath[MAXPATH];
            
            subfd = open(path, 0);
            if(subfd >= 0) {
                while(read(subfd, &subde, sizeof(subde)) == sizeof(subde)) {
                    if(subde.inum == 0 || strcmp(subde.name, ".") == 0 || strcmp(subde.name, "..") == 0)
                        continue;
                    
                    strcpy(subpath, path);
                    append_path(subpath, "/");
                    append_path(subpath, subde.name);
                    unlink(subpath);
                }
                close(subfd);
            }
        }
        unlink(path);
    }
    close(fd);
    printf("Corbeille vidée avec succès\n");
}

// Autres fonctions du gestionnaire de fichiers
void save_action(int type, char* path, char* oldname, char* newname) {
    history[history_index].type = type;
    strcpy(history[history_index].path, path);
    if(oldname) strcpy(history[history_index].oldname, oldname);
    if(newname) strcpy(history[history_index].newname, newname);
    history_index = (history_index + 1) % HISTORY_SIZE;
}



void copy_to_clipboard(char* name, int cut_mode) {
    struct stat st;
    
    if(stat(name, &st) < 0) {
        printf("Erreur: Element introuvable\n");
        return;
    }
    
    clipboard.mode = cut_mode ? 2 : 1; // 2 pour couper, 1 pour copier
    strcpy(clipboard.path, current_path);
    strcpy(clipboard.name, name);
    clipboard.isDir = (st.type == T_DIR) ? 1 : 0;
    clipboard.is_persistent = 1; // Rendre la copie persistante
    
    printf("%s %s dans le presse-papier\n", cut_mode ? "Coupé" : "Copié", name);
}

// Fonction pour coller un élément
void paste_from_clipboard() {
    if(clipboard.mode == 0) {
        printf("Le presse-papier est vide\n");
        return;
    }
    
    char src_path[MAXPATH], dst_path[MAXPATH];
    
    // Construire les chemins complets
    strcpy(src_path, clipboard.path);
    if(src_path[strlen(src_path)-1] != '/') {
        append_path(src_path, "/");
    }
    append_path(src_path, clipboard.name);
    
    strcpy(dst_path, current_path);
    if(dst_path[strlen(dst_path)-1] != '/') {
        append_path(dst_path, "/");
    }
    append_path(dst_path, clipboard.name);
    
    // Vérifier si le fichier existe déjà à destination
    struct stat st;
    if(stat(dst_path, &st) == 0) {
        printf("Un élément du même nom existe déjà. Voulez-vous le remplacer ? (o/n): ");
        char answer[2];
        gets(answer, 2);
        if(answer[0] != 'o' && answer[0] != 'O') {
            printf("Opération annulée\n");
            return;
        }
    }
    
    if(clipboard.isDir) {
        if(clipboard.mode == 2) {
            rename_directory(src_path, dst_path);
        } else {
            // Pour la copie de répertoire, on doit le faire manuellement
            if(mkdir(dst_path) < 0) {
                printf("Erreur lors de la création du répertoire destination\n");
                return;
            }
            // Copier récursivement le contenu
            // Note: Cette partie utilise rename_directory qui gère déjà la copie récursive
            rename_directory(src_path, dst_path);
            if(clipboard.mode == 1) {
                // Si c'était une copie, restaurer l'original qui a été déplacé
                rename_directory(dst_path, src_path);
                // Et le recopier
                rename_directory(src_path, dst_path);
            }
        }
    } else {
        copy_file(src_path, dst_path);
        if(clipboard.mode == 2) {
            unlink(src_path);
        }
    }
    
    if(clipboard.mode == 2) {
        // Vider le presse-papier uniquement si c'était une opération couper
        clipboard.mode = 0;
        clipboard.is_persistent = 0;
    }
    
    printf("Collé avec succès\n");
}

// Fonctions à ajouter dans le menu principal
void handle_copy() {
    printf("Nom de l'élément à copier: ");
    char name[MAXLEN];
    gets(name, MAXLEN);
    copy_to_clipboard(name, 0);
}

void handle_cut() {
    printf("Nom de l'élément à couper: ");
    char name[MAXLEN];
    gets(name, MAXLEN);
    copy_to_clipboard(name, 1);
}

void handle_paste() {
    paste_from_clipboard();
}



void list_files() {
    int fd;
    struct dirent de;
    struct stat st;
    
    fd = open(".", 0);
    if(fd < 0) {
        printf("Impossible d'ouvrir le répertoire\n");
        return;
    }
    
    printf("\nContenu du répertoire %s:\n", current_path);
    while(read(fd, &de, sizeof(de)) == sizeof(de)) {
        if(de.inum == 0)
            continue;
        if(stat(de.name, &st) < 0) {
            printf("Impossible de lire %s\n", de.name);
            continue;
        }
        if(st.type == T_DIR)
            printf("[DIR] %s\n", de.name);
        else
            printf("[FILE] %s (%ld bytes)\n", de.name, st.size);
    }
    close(fd);
}

void create_file() {
    char buf[512];
    char filename[32];
    char temp[2];
    int i = 0;
    int fd;
    int line_start = 0;
    
    // Demander le nom du fichier
    printf("Nom du fichier a creer: ");
    while(i < sizeof(filename) - 1) {
        if(read(0, &filename[i], 1) != 1)
            break;
        if(filename[i] == '\n') {
            filename[i] = 0;
            break;
        }
        i++;
    }

    // Ouvrir le fichier
    fd = open(filename, O_CREATE | O_WRONLY);
    if(fd < 0) {
        printf("Erreur: impossible de creer le fichier\n");
        return;
    }

    printf("Entrez le contenu du fichier (ligne vide pour terminer):\n");
    
    i = 0;
    while(1) {
        printf("> ");
        line_start = i;  // Mémoriser le début de la ligne
        
        // Lire la ligne
        while(i < sizeof(buf) - 1) {
            if(read(0, &temp[0], 1) != 1)
                break;
                
            if(temp[0] == '\n') {
                buf[i] = temp[0];
                i++;
                // Si la ligne est vide (rien entre le prompt et \n)
                if(i == line_start + 1) {
                    i = line_start;  // Ne pas inclure la ligne vide
                    goto end_input;
                }
                break;
            }
            
            buf[i] = temp[0];
            i++;
        }
    }

end_input:
    buf[i] = 0;  // Terminer le buffer
    
    printf("Voulez-vous sauvegarder le fichier ? (o/n): ");
    read(0, temp, 1);
    read(0, &temp[1], 1);  // Lire le \n
    
    if(temp[0] == 'o' || temp[0] == 'O') {
        write(fd, buf, strlen(buf));
        printf("Fichier sauvegarde avec succes!\n");
    } else {
        printf("Operation annulee\n");
    }
    
    close(fd);
}

void create_directory() {
    char dirname[32];  // Taille raisonnable pour un nom de répertoire
    int i = 0;
    
    // Demander le nom du répertoire
    printf("Nom du repertoire a creer: ");
    while(i < sizeof(dirname) - 1) {
        if(read(0, &dirname[i], 1) != 1)
            break;
        if(dirname[i] == '\n') {
            dirname[i] = 0;
            break;
        }
        i++;
    }

    // Créer le répertoire
    if(mkdir(dirname) < 0) {
        printf("Erreur: impossible de creer le repertoire\n");
        return;
    }

    printf("Repertoire '%s' cree avec succes!\n", dirname);
}


void undo_last_action() {
    if(history_index == 0) {
        printf("Aucune action à annuler\n");
        return;
    }
    
    int last_index = (history_index - 1 + HISTORY_SIZE) % HISTORY_SIZE;
    struct Action* last = &history[last_index];
    
    switch(last->type) {
        case 1:
            printf("Impossible de restaurer les fichiers supprimés dans xv6\n");
            break;
        case 2:
            strcpy(current_path, last->path);
            if(chdir(current_path) < 0)
                printf("Erreur lors du retour au répertoire %s\n", current_path);
            else
                printf("Retour au répertoire %s\n", current_path);
            break;
    }
    
    history_index = last_index;
}




void save_to_back_history(char *path) {
    if (history_active) return; // Ne pas enregistrer pendant la navigation historique
    strcpy(back_history[back_index], path);
    back_index = (back_index + 1) % HISTORY_SIZE;
    forward_index = 0; // Effacer l'historique avant lorsqu'on avance
}

// Fonction pour retourner en arrière
void go_back() {
    if (back_index == 0) {
        printf("Pas de répertoire précédent\n");
        return;
    }

    // Passer au répertoire précédent
    back_index = (back_index - 1 + HISTORY_SIZE) % HISTORY_SIZE;
    strcpy(forward_history[forward_index], current_path);
    forward_index = (forward_index + 1) % HISTORY_SIZE;

    // Changer le répertoire
    strcpy(current_path, back_history[back_index]);
    if (chdir(current_path) < 0) {
        printf("Erreur: Impossible d'accéder au répertoire précédent\n");
        forward_index = (forward_index - 1 + HISTORY_SIZE) % HISTORY_SIZE; // Annuler
    } else {
        history_active = 1; // Marquer comme navigation historique
        printf("Répertoire précédent : %s\n", current_path);
        history_active = 0;
    }
}

// Fonction pour aller en avant
void go_forward() {
    if (forward_index == 0 || strlen(forward_history[forward_index - 1]) == 0) {
        printf("Pas de répertoire suivant\n");
        return;
    }

    // Passer au répertoire suivant
    forward_index = (forward_index - 1 + HISTORY_SIZE) % HISTORY_SIZE;
    strcpy(back_history[back_index], current_path);
    back_index = (back_index + 1) % HISTORY_SIZE;

    // Changer le répertoire
    strcpy(current_path, forward_history[forward_index]);
    if (chdir(current_path) < 0) {
        printf("Erreur: Impossible d'accéder au répertoire suivant\n");
        back_index = (back_index - 1 + HISTORY_SIZE) % HISTORY_SIZE; // Annuler
    } else {
        history_active = 1; // Marquer comme navigation historique
        printf("Répertoire suivant : %s\n", current_path);
        history_active = 0;
    }
}


void create_archive(char *path) {
    char archive_name[MAXLEN];
    printf("Entrez le nom de l'archive: ");
    gets(archive_name, MAXLEN);

    // Ouvrir le fichier archive en écriture
    int fd_archive = open(archive_name, O_CREATE | O_WRONLY);
    if(fd_archive < 0) {
        printf("Erreur: impossible de créer l'archive\n");
        return;
    }

    // Ouvrir le répertoire source
    int fd_src = open(path, O_RDONLY);
    if(fd_src < 0) {
        printf("Erreur: impossible d'ouvrir le chemin source\n");
        close(fd_archive);
        return;
    }

    struct dirent de;
    char buf[512];
    struct stat st;

    // Parcourir le répertoire
    while(read(fd_src, &de, sizeof(de)) == sizeof(de)) {
        if(de.inum == 0) 
            continue;

        // Préparer le chemin complet du fichier
        char filepath[MAXLEN];
        strcpy(filepath, path);
        strcpy(filepath + strlen(filepath), "/");
        strcpy(filepath + strlen(filepath), de.name);

        if(stat(filepath, &st) < 0) {
            continue;
        }

        // Ignorer les répertoires
        if(st.type == T_DIR)
            continue;

        // Créer et écrire l'en-tête
        struct archive_header header;
        strcpy(header.name, de.name);
        header.size = st.size;
        write(fd_archive, &header, sizeof(header));

        // Copier le contenu du fichier
        int fd_file = open(filepath, O_RDONLY);
        if(fd_file < 0)
            continue;

        int n;
        while((n = read(fd_file, buf, sizeof(buf))) > 0) {
            write(fd_archive, buf, n);
        }
        close(fd_file);
    }

    close(fd_src);
    close(fd_archive);
}

void extract_archive(char *archive_name) {
    // Ouvrir l'archive en lecture
    int fd_archive = open(archive_name, O_RDONLY);
    if(fd_archive < 0) {
        printf("Erreur: impossible d'ouvrir l'archive\n");
        return;
    }

    struct archive_header header;
    char buf[512];

    // Lire chaque fichier de l'archive
    while(read(fd_archive, &header, sizeof(header)) == sizeof(header)) {
        // Créer le nouveau fichier
        int fd_out = open(header.name, O_CREATE | O_WRONLY);
        if(fd_out < 0) {
            printf("Erreur: impossible de créer %s\n", header.name);
            continue;
        }

        // Copier le contenu
        uint remaining = header.size;
        while(remaining > 0) {
            int to_read = remaining;
            if(to_read > sizeof(buf))
                to_read = sizeof(buf);
                
            int n = read(fd_archive, buf, to_read);
            if(n <= 0)
                break;
            write(fd_out, buf, n);
            remaining -= n;
        }

        close(fd_out);
    }

    close(fd_archive);
}


void search_file_in_dir(char *path, char *filename) {
    char buf[512], *p;
    int fd;
    struct dirent de;
    struct stat st;

    if((fd = open(path, 0)) < 0) {
        printf("Impossible d'ouvrir le répertoire %s\n", path);
        return;
    }

    if(strlen(path) + 1 + DIRSIZ + 1 > sizeof buf) {
        printf("Chemin trop long\n");
        close(fd);
        return;
    }

    // Copier le chemin actuel
    strcpy(buf, path);
    p = buf + strlen(buf);
    *p++ = '/';

    // Lire le contenu du répertoire
    while(read(fd, &de, sizeof(de)) == sizeof(de)) {
        if(de.inum == 0)
            continue;

        // Copier le nom du fichier/dossier
        memmove(p, de.name, DIRSIZ);
        p[DIRSIZ] = 0;

        if(stat(buf, &st) < 0) {
            printf("Impossible d'obtenir les stats de %s\n", buf);
            continue;
        }

        // Si c'est un fichier et que le nom correspond
        if(st.type == T_FILE) {
            if(strcmp(de.name, filename) == 0) {
                printf("Trouvé: %s\n", buf);
            }
        }
        // Si c'est un répertoire, rechercher récursivement
        else if(st.type == T_DIR && strcmp(de.name, ".") != 0 && strcmp(de.name, "..") != 0) {
            search_file_in_dir(buf, filename);
        }
    }

    close(fd);
}


void display_file_content() {
    char filename[MAXLEN];
    char buf[512];
    int fd, n;
    
    printf("Nom du fichier à afficher: ");
    gets(filename, MAXLEN);
    
    fd = open(filename, O_RDONLY);
    if(fd < 0) {
        printf("Erreur: impossible d'ouvrir le fichier\n");
        return;
    }
    
    while((n = read(fd, buf, sizeof(buf))) > 0) {
        write(1, buf, n);  // 1 est le descripteur de fichier pour stdout
    }
    
    close(fd);
}



// Fonction principale de recherche
void find(char *filename) {
    printf("Recherche de '%s'...\n", filename);
    search_file_in_dir(".", filename);
}





void print_welcome_message() {
    printf("\033[1;36m************************************************************\n");
    printf("*                                                          *\n");
    printf("*   🚀✨ BIENVENUE SUR KALI-xv6 : L'AVENIR EST ICI ✨🚀    *\n");
    printf("*                                                          *\n");
    printf("*   Une version révolutionnaire de xv6, pensée et conçue   *\n");
    printf("*   par les brillants esprits de l'Université de Yaoundé I, *\n");
    printf("*   sous la direction éclairée du Dr. Adamou Hamza          *\n");
    printf("*             et de M. Ngouanffo Gildas.                    *\n");
    printf("*                                                          *\n");
    printf("*   🧠 Explorez les limites de l'innovation,               *\n");
    printf("*   🔥 Déchaînez votre créativité,                         *\n");
    printf("*   🌍 Et façonnez le futur dès aujourd'hui !              *\n");
    printf("*                                                          *\n");
    printf("*         >>> Préparez-vous à repousser les frontières <<< *\n");
    printf("*                                                          *\n");
    printf("************************************************************\033[0m\n\n");
}

void print_menu_header(const char* title) {
    printf("\033[1;34m╔══════════════ %s ══════════════╗\033[0m\n", title);
}

void print_menu_footer() {
    printf("\033[1;34m╚═══════════════════════════════════════╝\033[0m\n");
}

void print_menu_item(int number, const char* text) {
    printf("\033[1;32m│\033[0m %d. %s\n", number, text);
}

void print_current_path() {
    printf("\033[1;33m📂 %s\033[0m\n\n", current_path);
}

void print_main_menu() {
    clear_screen();
    print_current_path();
    print_menu_header("Menu Principal");
    print_menu_item(1, "📋 Afficher le contenu du répertoire");
    print_menu_item(2, "🗺️  Navigation");
    print_menu_item(3, "📁 Fichiers et répertoires");
    print_menu_item(4, "📋 Opérations de copie");
    print_menu_item(5, "🗑️  Corbeille");
    print_menu_item(6, "📦 Archives");
    print_menu_item(0, "❌ Quitter");
    print_menu_footer();
    printf("\nChoix: ");
}

void print_navigation_menu() {
    clear_screen();
    print_current_path();
    print_menu_header("Navigation");
    print_menu_item(1, "📂 Accéder à un répertoire");
    print_menu_item(2, "⬅️  Retour au répertoire précédent");
    print_menu_item(3, "➡️  Avancer au répertoire suivant");
    print_menu_item(4, "🏠 Retour au menu principal");
    print_menu_footer();
    printf("\nChoix: ");
}

void print_files_menu() {
    clear_screen();
    print_current_path();
    print_menu_header("Fichiers et Répertoires");
    print_menu_item(1, "📝 Créer un fichier");
    print_menu_item(2, "📁 Créer un répertoire");
    print_menu_item(3, "✏️  Renommer un fichier");
    print_menu_item(4, "✏️  Renommer un répertoire");
    print_menu_item(5, "🗑️  Supprimer un fichier");
    print_menu_item(6, "🗑️  Supprimer un répertoire");
    print_menu_item(7, "🔍 Rechercher un fichier");
    print_menu_item(8, "🏠 Retour au menu principal");
    print_menu_footer();
    printf("\nChoix: ");
}

void print_copy_menu() {
    clear_screen();
    print_current_path();
    print_menu_header("Copier/Couper/Coller");
    print_menu_item(1, "📋 Copier un élément");
    print_menu_item(2, "✂️  Couper un élément");
    print_menu_item(3, "📌 Coller");
    print_menu_item(4, "🏠 Retour au menu principal");
    print_menu_footer();
    printf("\nChoix: ");
}

void print_trash_menu() {
    clear_screen();
    print_current_path();
    print_menu_header("Corbeille");
    print_menu_item(1, "📋 Lister le contenu");
    print_menu_item(2, "♻️  Restaurer un fichier");
    print_menu_item(3, "🗑️  Vider la corbeille");
    print_menu_item(4, "🏠 Retour au menu principal");
    print_menu_footer();
    printf("\nChoix: ");
}

void print_archive_menu() {
    clear_screen();
    print_current_path();
    print_menu_header("Archives");
    print_menu_item(1, "📦 Créer une archive");
    print_menu_item(2, "📂 Extraire une archive");
    print_menu_item(3, "🏠 Retour au menu principal");
    print_menu_footer();
    printf("\nChoix: ");
}

// Modification du main pour inclure les nouveaux menus
int main() {
    char choice[MAXLEN];
    char name[MAXLEN];
    char newname[MAXLEN];
    char filename[MAXLEN];
    
    strcpy(current_path, "/");
    print_welcome_message();
    printf("Appuyez sur Entrée pour continuer...");
    gets(name, MAXLEN);
    
    while(1) {
        print_main_menu();
        gets(choice, MAXLEN);
        
        switch(choice[0]) {
            case '1':
                clear_screen();
                list_files();
                printf("\nAppuyez sur Entrée pour continuer...");
                gets(name, MAXLEN);
                break;
                
            case '2': // Menu Navigation
                while(1) {
                    print_navigation_menu();
                    gets(choice, MAXLEN);
                    
                    if(choice[0] == '1') {
                        strcpy(previous_path, current_path);
                        printf("Nom du répertoire à accéder: ");
                        gets(name, MAXLEN);
                        if(chdir(name) < 0)
                            printf("Impossible d'accéder au répertoire\n");
                        else {
                            if(name[0] == '/') {
                                strcpy(current_path, name);
                            } else {
                                if(strcmp(current_path, "/") != 0) {
                                    append_path(current_path, "/");
                                }
                                append_path(current_path, name);
                            }
                            save_action(2, previous_path, 0, 0);
                            save_to_back_history(previous_path);
                        }
                    }
                    else if(choice[0] == '2') {
                        go_back();
                    }
                    else if(choice[0] == '3') {
                        go_forward();
                    }
                    else if(choice[0] == '4') {
                        break;
                    }
                    printf("\nAppuyez sur Entrée pour continuer...");
                    gets(name, MAXLEN);
                }
                break;
                
            case '3': // Menu Fichiers et Répertoires
                while(1) {
                    print_files_menu();
                    gets(choice, MAXLEN);
                    
                    if(choice[0] == '1') {
                        create_file();
                    }
                    else if(choice[0] == '2') {
                        printf("Nom du nouveau répertoire: ");
                        gets(name, MAXLEN);
                        if(mkdir(name) < 0)
                            printf("Impossible de créer le répertoire\n");
                        else
                            printf("Répertoire créé avec succès\n");
                    }
                    else if(choice[0] == '3') {
                        printf("Nom du fichier à renommer: ");
                        gets(name, MAXLEN);
                        printf("Nouveau nom: ");
                        gets(newname, MAXLEN);
                        rename_file(name, newname);
                        save_action(3, current_path, name, newname);
                    }
                    else if(choice[0] == '4') {
                        printf("Nom du répertoire à renommer: ");
                        gets(name, MAXLEN);
                        printf("Nouveau nom: ");
                        gets(newname, MAXLEN);
                        rename_directory(name, newname);
                        save_action(3, current_path, name, newname);
                    }
                    else if(choice[0] == '5') {
                        printf("Nom du fichier à supprimer: ");
                        gets(name, MAXLEN);
                        move_to_trash(name);
                        save_action(1, current_path, name, 0);
                    }
                    else if(choice[0] == '6') {
                        printf("Nom du répertoire à supprimer: ");
                        gets(name, MAXLEN);
                        move_to_trash(name);
                        save_action(1, current_path, name, 0);
                    }
                    else if(choice[0] == '7') {
                        printf("Nom du fichier à rechercher: ");
                        gets(filename, MAXLEN);
                        find(filename);
                    }
                    else if(choice[0] == '8') {
            		display_file_content();
        		}
        	    else if(choice[0] == '9') {  // Modifié de 8 à 9
            		break;
        		}
                    printf("\nAppuyez sur Entrée pour continuer...");
                    gets(name, MAXLEN);
                }
                break;
                
            case '4': // Menu Copier/Couper/Coller
                while(1) {
                    print_copy_menu();
                    gets(choice, MAXLEN);
                    
                    if(choice[0] == '1') {
                        handle_copy();
                    }
                    else if(choice[0] == '2') {
                        handle_cut();
                    }
                    else if(choice[0] == '3') {
                        handle_paste();
                    }
                    else if(choice[0] == '4') {
                        break;
                    }
                    printf("\nAppuyez sur Entrée pour continuer...");
                    gets(name, MAXLEN);
                }
                break;
            case '5': // Menu Corbeille
    while(1) {
        print_trash_menu();
        gets(choice, MAXLEN);
        
        if(choice[0] == '1') {
            clear_screen();
            list_files();
            printf("\nAppuyez sur Entrée pour continuer...");
            gets(name, MAXLEN);
        }
        else if(choice[0] == '2') {
            printf("Nom du fichier à restaurer: ");
            gets(name, MAXLEN);
        }
        else if(choice[0] == '3') {
            empty_trash();
        }
        else if(choice[0] == '4') {
            break;
        }
        printf("\nAppuyez sur Entrée pour continuer...");
        gets(name, MAXLEN);
    }
    break;
            
            case '6': // Menu Archives
                while(1) {
                    print_archive_menu();
                    gets(choice, MAXLEN);
                    
                    if(choice[0] == '1') {
                        printf("Chemin à archiver: ");
                        gets(name, MAXLEN);
                        create_archive(name);
                    }
                    else if(choice[0] == '2') {
                        printf("Nom de l'archive à extraire: ");
                        gets(name, MAXLEN);
                        extract_archive(name);
                    }
                    else if(choice[0] == '3') {
                        break;
                    }
                    printf("\nAppuyez sur Entrée pour continuer...");
                    gets(name, MAXLEN);
                }
                break;
                
            case '0':
                exit(0);
                
            default:
                printf("\033[1;31mOption invalide!\033[0m\n");
                printf("\nAppuyez sur Entrée pour continuer...");
                gets(name, MAXLEN);
                break;
        }
    }
}
