#include <stdio.h>
#include <unistd.h> // Para fork() e getpid()
#include <sys/types.h> // Para tipos como pid_t
#include <sys/wait.h> //Para waitpid()
#include <stdlib.h> // Para NULL

int main(void) {

    char var = 1;
    printf("Valor inicial de var: %d\n", var);

    pid_t filho;

    filho = fork();
    if (filho < 0) {
        puts("Erro ao criar processo filho.");
        exit(-1);
    }

    if (filho != 0) {
        // Processo pai
        printf("PID do processo pai: %d\n", getpid());
        waitpid(-1, NULL, 0); // Espera o filho terminar

        printf("Valor de var no processo pai: %d\n", var);

        printf("Processo pai encerrando.\n");
        exit(0);
    }

    // Processo filho
    printf("PID do processo filho: %d\n", getpid());
    
    var = 5;
    printf("Valor de var no processo filho: %d\n", var);
    
    printf("Processo filho encerrando.\n");
    
    exit(0);
}

/* Os valores devem ser diferentes pois cada processo (pai e filho) tem uma pilha de váriaveis locais independente um do outro.*/