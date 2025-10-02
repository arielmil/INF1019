#include <stdio.h>
#include <unistd.h> // Para fork() e getpid()
#include <sys/types.h> // Para tipos como pid_t
#include <sys/wait.h> //Para waitpid()
#include <stdlib.h> // Para NULL

int main(int argc, char *argv[]) {
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

        exit(0);
    }

    // Processo filho
    printf("PID do processo filho: %d\n", getpid());

    execv("./helloworld", argv);
    
    /* Filho nunca chegará aqui pois execve sobreescreve o código que o filho estava rodando pelo código do programa passado para a chamada de serviço. */
    printf("Processo filho encerrando.\n");
    
    exit(0);
}