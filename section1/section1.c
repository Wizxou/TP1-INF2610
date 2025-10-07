#include <stdio.h>
#include <stdlib.h>
#include <unistd.h>
#include <fcntl.h>
#include <string.h>
#include <sys/wait.h>
#include <sys/types.h>

int main(int argc, char *argv[]) {

    int fichier = open("./utils/mots.txt", O_RDONLY);
    if (fichier == -1) {
        perror("Erreur ouverture fichier");
        exit(1);
    }
    
    char buffer[10000];
    int nb_bytes = read(fichier, buffer, sizeof(buffer));
    close(fichier);
    
    int mots = 0;
    int i = 0;
    int dans_mot = 0;
    
    while (i < nb_bytes) {
        char c = buffer[i];
        
        if (c != ' ' && c != '\t' && c != '\n' && c != '\r') {
            if (dans_mot == 0) {
                mots++;
                dans_mot = 1;
            }
        } else {
            dans_mot = 0;
        }
        i++;
    }
    
    int resultat = open("section1_2.txt", O_WRONLY | O_CREAT | O_TRUNC, 0644);
    if (resultat == -1) {
        perror("Erreur création fichier");
        exit(1);
    }
    
    char nombre[20];
    sprintf(nombre, "%d", mots);
    write(resultat, nombre, strlen(nombre));
    close(resultat);
    
    return 0;
}
