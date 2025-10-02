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

    // Modificado para execvp para buscar o executável no PATH

    char *args[] = {"echo", "Hello, World!", NULL};
    execvp("echo", args);
    
    /* Filho nunca chegará aqui pois execve sobreescreve o código que o filho estava rodando pelo código do programa passado para a chamada de serviço. */
    printf("Processo filho encerrando.\n");
    
    exit(0);
}

/*

    args possúi como primeiro elemento o nome do programa que será executado, como demais argumentos
    (com exeção do ultimo, os argumentos que seriam passados via linha de comando) e NULL como ultimo elemento, para indicar o fim dos argumentos.

    Essa chamada é equivalente a fazer no terminal: echo "Hello, World!", repare que o primeiro argumento é de fato o nome do programa.

*/