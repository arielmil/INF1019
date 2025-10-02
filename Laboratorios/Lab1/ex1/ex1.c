#include <stdio.h>
#include <unistd.h> // Para fork() e getpid()
#include <sys/types.h> // Para tipos como pid_t
#include <sys/wait.h> //Para waitpid()
#include <stdlib.h> // Para NULL


int main(void) {

    pid_t filho;

    filho = fork();
    if (filho < 0) {
        puts("Erro ao criar processo filho.");
        return -1;
    }

    if (filho != 0) {
        // Processo pai
        printf("PID do processo pai: %d\n", getpid());
        waitpid(-1, NULL, 0); // Espera o filho terminar
        printf("Processo pai encerrando.\n");
        return 0;
    }

    // Processo filho
    printf("PID do processo filho: %d\n", getpid());
    printf("Processo filho encerrando.\n");
    return 0;
}