#include <stdio.h>
#include <stdlib.h>
#include <unistd.h>
#include <fcntl.h>
#include <string.h>
#include <sys/wait.h>
#include <sys/types.h>
#include <sys/stat.h>

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
    
    int pipes1[n][2];
    int pipes2[n][2];
    
    for (int i = 0; i < n; i++) {
        if (pipe(pipes1[i]) == -1 || pipe(pipes2[i]) == -1) {
            perror("Erreur pipe");
            exit(1);
        }
    }
    
    for (int i = 0; i < n; i++) {
        pid_t pid = fork();
        if (pid == 0) {
            for (int j = 0; j < n; j++) {
                close(pipes1[j][0]);
                if (j != i) close(pipes1[j][1]);
                close(pipes2[j][0]);
                close(pipes2[j][1]);
            }
            
            int fd = open(log1, O_RDONLY);
            if (fd == -1) {
                perror("Erreur ouverture fichier");
                exit(1);
            }
            
            off_t debut = i * taille_bloc1;
            off_t fin = (i == n - 1) ? taille1 : (i + 1) * taille_bloc1;
            off_t taille = fin - debut;
            
            lseek(fd, debut, SEEK_SET);
            
            char *buffer = malloc(taille + 1);
            ssize_t bytes_lus = read(fd, buffer, taille);
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
            
            char pid_msg[50];
            sprintf(pid_msg, "PID: %d\n", getpid());
            ssize_t ret = write(STDOUT_FILENO, pid_msg, strlen(pid_msg));
            (void)ret;
            
            int results[3] = {critical, error, failed_login};
            ret = write(pipes1[i][1], results, sizeof(results));
            (void)ret;
            close(pipes1[i][1]);
            free(buffer);
            exit(0);
        }
    }
    
    for (int i = 0; i < n; i++) {
        pid_t pid = fork();
        if (pid == 0) {
            for (int j = 0; j < n; j++) {
                close(pipes1[j][0]);
                close(pipes1[j][1]);
                close(pipes2[j][0]);
                if (j != i) close(pipes2[j][1]);
            }
            
            int fd = open(log2, O_RDONLY);
            if (fd == -1) {
                perror("Erreur ouverture fichier");
                exit(1);
            }
            
            off_t debut = i * taille_bloc2;
            off_t fin = (i == n - 1) ? taille2 : (i + 1) * taille_bloc2;
            off_t taille = fin - debut;
            
            lseek(fd, debut, SEEK_SET);
            
            char *buffer = malloc(taille + 1);
            ssize_t bytes_lus = read(fd, buffer, taille);
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
            
            char pid_msg[50];
            sprintf(pid_msg, "PID: %d\n", getpid());
            ssize_t ret = write(STDOUT_FILENO, pid_msg, strlen(pid_msg));
            (void)ret;
            
            int results[3] = {critical, error, failed_login};
            ret = write(pipes2[i][1], results, sizeof(results));
            (void)ret;
            close(pipes2[i][1]);
            free(buffer);
            exit(0);
        }
    }
    
    for (int i = 0; i < n; i++) {
        close(pipes1[i][1]);
        close(pipes2[i][1]);
    }
    
    for (int i = 0; i < 2 * n; i++) {
        wait(NULL);
    }
    
    int total_critical1 = 0, total_error1 = 0, total_failed_login1 = 0;
    int total_critical2 = 0, total_error2 = 0, total_failed_login2 = 0;
    
    for (int i = 0; i < n; i++) {
        int results[3];
        ssize_t ret = read(pipes1[i][0], results, sizeof(results));
        (void)ret;
        total_critical1 += results[0];
        total_error1 += results[1];
        total_failed_login1 += results[2];
        close(pipes1[i][0]);
    }
    
    for (int i = 0; i < n; i++) {
        int results[3];
        ssize_t ret = read(pipes2[i][0], results, sizeof(results));
        (void)ret;
        total_critical2 += results[0];
        total_error2 += results[1];
        total_failed_login2 += results[2];
        close(pipes2[i][0]);
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
    
    int fd_result = open("RESULT_PROCESS.txt", O_WRONLY | O_CREAT | O_TRUNC, 0644);
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
    
    return 0;
}
