#include <stdio.h>
#include <stdlib.h>
#include <unistd.h>
#include <fcntl.h>
#include <string.h>
#include <pthread.h>
#include <sys/types.h>
#include <sys/stat.h>

struct thread_args {
    char *nom_fichier;
    off_t debut;
    off_t taille;
    int critical;
    int error;
    int failed_login;
    pthread_t tid;
};

void *analyser_bloc(void *arg) {
    struct thread_args *args = (struct thread_args *)arg;
    
    int fd = open(args->nom_fichier, O_RDONLY);
    if (fd == -1) {
        perror("Erreur ouverture fichier");
        pthread_exit(NULL);
    }
    
    char *buffer = malloc(args->taille + 1);
    if (buffer == NULL) {
        perror("Erreur malloc");
        close(fd);
        pthread_exit(NULL);
    }
    
    ssize_t bytes_lus = pread(fd, buffer, args->taille, args->debut);
    buffer[bytes_lus] = '\0';
    close(fd);
    
    int critical = 0, error = 0, failed_login = 0;
    
    char *pos = buffer;
    while ((pos = strstr(pos, "level=CRITICAL")) != NULL) {
        critical++;
        pos++;
    }
    
    pos = buffer;
    while ((pos = strstr(pos, "level=ERROR")) != NULL) {
        error++;
        pos++;
    }
    
    pos = buffer;
    while ((pos = strstr(pos, "level=FAILED_LOGIN")) != NULL) {
        failed_login++;
        pos++;
    }
    
    free(buffer);
    
    args->critical = critical;
    args->error = error;
    args->failed_login = failed_login;
    
    pthread_exit(NULL);
}

int main(int argc, char *argv[]) {
    char *log1, *log2;
    int n;
    
    if (argc < 4) {
        log1 = "logs.txt";
        log2 = "logs_2.txt";
        n = 2;
    } else {
        log1 = argv[1];
        log2 = argv[2];
        n = atoi(argv[3]);
    }
    
    if (n < 1) {
        printf("Le nombre de blocs doit être >= 1\n");
        return 1;
    }
    
    struct stat st1, st2;
    if (stat(log1, &st1) == -1 || stat(log2, &st2) == -1) {
        perror("Erreur stat");
        exit(1);
    }
    
    off_t taille1 = st1.st_size;
    off_t taille2 = st2.st_size;
    off_t taille_bloc1 = taille1 / n;
    off_t taille_bloc2 = taille2 / n;
    
    struct thread_args *threads1 = malloc(n * sizeof(struct thread_args));
    struct thread_args *threads2 = malloc(n * sizeof(struct thread_args));
    
    for (int i = 0; i < n; i++) {
        threads1[i].nom_fichier = log1;
        threads1[i].debut = i * taille_bloc1;
        threads1[i].taille = (i == n - 1) ? (taille1 - threads1[i].debut) : taille_bloc1;
        threads1[i].critical = 0;
        threads1[i].error = 0;
        threads1[i].failed_login = 0;
        
        pthread_create(&threads1[i].tid, NULL, analyser_bloc, &threads1[i]);
    }
    
    for (int i = 0; i < n; i++) {
        threads2[i].nom_fichier = log2;
        threads2[i].debut = i * taille_bloc2;
        threads2[i].taille = (i == n - 1) ? (taille2 - threads2[i].debut) : taille_bloc2;
        threads2[i].critical = 0;
        threads2[i].error = 0;
        threads2[i].failed_login = 0;
        
        pthread_create(&threads2[i].tid, NULL, analyser_bloc, &threads2[i]);
    }
    
    for (int i = 0; i < n; i++) {
        pthread_join(threads1[i].tid, NULL);
    }
    for (int i = 0; i < n; i++) {
        pthread_join(threads2[i].tid, NULL);
    }
    
    for (int i = 0; i < n; i++) {
        char tid_msg[50];
        sprintf(tid_msg, "TID: %lu\n", (unsigned long)threads1[i].tid);
        ssize_t ret = write(STDOUT_FILENO, tid_msg, strlen(tid_msg));
        (void)ret;
    }
    for (int i = 0; i < n; i++) {
        char tid_msg[50];
        sprintf(tid_msg, "TID: %lu\n", (unsigned long)threads2[i].tid);
        ssize_t ret = write(STDOUT_FILENO, tid_msg, strlen(tid_msg));
        (void)ret;
    }
    
    int total_critical1 = 0, total_error1 = 0, total_failed_login1 = 0;
    int total_critical2 = 0, total_error2 = 0, total_failed_login2 = 0;
    
    for (int i = 0; i < n; i++) {
        total_critical1 += threads1[i].critical;
        total_error1 += threads1[i].error;
        total_failed_login1 += threads1[i].failed_login;
    }
    
    for (int i = 0; i < n; i++) {
        total_critical2 += threads2[i].critical;
        total_error2 += threads2[i].error;
        total_failed_login2 += threads2[i].failed_login;
    }
    
    int total_critical = total_critical1 + total_critical2;
    int total_error = total_error1 + total_error2;
    int total_failed_login = total_failed_login1 + total_failed_login2;
    
    char msg[200];
    ssize_t ret;
    sprintf(msg, "%s : CRITICAL : %d\n", log1, total_critical1);
    ret = write(STDOUT_FILENO, msg, strlen(msg));
    (void)ret;
    sprintf(msg, "%s : ERROR : %d\n", log1, total_error1);
    ret = write(STDOUT_FILENO, msg, strlen(msg));
    (void)ret;
    sprintf(msg, "%s : FAILED_LOGIN : %d\n", log1, total_failed_login1);
    ret = write(STDOUT_FILENO, msg, strlen(msg));
    (void)ret;
    
    sprintf(msg, "%s : CRITICAL : %d\n", log2, total_critical2);
    ret = write(STDOUT_FILENO, msg, strlen(msg));
    (void)ret;
    sprintf(msg, "%s : ERROR : %d\n", log2, total_error2);
    ret = write(STDOUT_FILENO, msg, strlen(msg));
    (void)ret;
    sprintf(msg, "%s : FAILED_LOGIN : %d\n", log2, total_failed_login2);
    ret = write(STDOUT_FILENO, msg, strlen(msg));
    (void)ret;
    
    sprintf(msg, "Total CRITICAL : %d\n", total_critical);
    ret = write(STDOUT_FILENO, msg, strlen(msg));
    (void)ret;
    sprintf(msg, "Total ERROR : %d\n", total_error);
    ret = write(STDOUT_FILENO, msg, strlen(msg));
    (void)ret;
    sprintf(msg, "Total FAILED_LOGIN : %d\n", total_failed_login);
    ret = write(STDOUT_FILENO, msg, strlen(msg));
    (void)ret;
    
    int fd_result = open("RESULT_THREADS.txt", O_WRONLY | O_CREAT | O_TRUNC, 0644);
    if (fd_result == -1) {
        perror("Erreur creation fichier");
        exit(1);
    }
    
    sprintf(msg, "%s : CRITICAL : %d\n", log1, total_critical1);
    ret = write(fd_result, msg, strlen(msg));
    (void)ret;
    sprintf(msg, "%s : ERROR : %d\n", log1, total_error1);
    ret = write(fd_result, msg, strlen(msg));
    (void)ret;
    sprintf(msg, "%s : FAILED_LOGIN : %d\n", log1, total_failed_login1);
    ret = write(fd_result, msg, strlen(msg));
    (void)ret;
    
    sprintf(msg, "%s : CRITICAL : %d\n", log2, total_critical2);
    ret = write(fd_result, msg, strlen(msg));
    (void)ret;
    sprintf(msg, "%s : ERROR : %d\n", log2, total_error2);
    ret = write(fd_result, msg, strlen(msg));
    (void)ret;
    sprintf(msg, "%s : FAILED_LOGIN : %d\n", log2, total_failed_login2);
    ret = write(fd_result, msg, strlen(msg));
    (void)ret;
    
    sprintf(msg, "Total CRITICAL : %d\n", total_critical);
    ret = write(fd_result, msg, strlen(msg));
    (void)ret;
    sprintf(msg, "Total ERROR : %d\n", total_error);
    ret = write(fd_result, msg, strlen(msg));
    (void)ret;
    sprintf(msg, "Total FAILED_LOGIN : %d\n", total_failed_login);
    ret = write(fd_result, msg, strlen(msg));
    (void)ret;
    
    close(fd_result);
    
    free(threads1);
    free(threads2);
    
    return 0;
}
