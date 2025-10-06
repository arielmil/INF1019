#include <stdio.h>
#include <stdlib.h> // Para exit e _exit
#include <signal.h> // Para kill() e signal()
#include <sys/types.h> // Para tipos como pid_t
#include <unistd.h> // Para fork(), execve() e sleep()
#include <sys/wait.h> // Para waitpid()

pid_t filhos[2];

void childHandler(int signo) {
    char string[] = "Um dos filhos foi interrompido. Saindo do programa...\n";

    write(STDOUT_FILENO, string, sizeof(string) - 1);
    _exit(0);
}

int main(void) {
    int vezesContinuadas;

    filhos[0] = fork();
    if (filhos[0] < 0) {
        perror("Erro ao criar filho 1. Saindo...");
        exit(-1);
    }

    if(filhos[0] != 0) {
        // Area do pai

        kill(filhos[0], SIGSTOP);

        filhos[1] = fork();
        if (filhos[1] < 0) {
            perror("Erro ao criar filho 2. Saindo...");

            kill(filhos[0], SIGINT);
            exit(-2);
        }

        if (filhos[1] != 0) {
            // Manda sinal de SIGSTOP para que filho1 comece no estado de STOP
            kill(filhos[1], SIGSTOP);
        }
    }

    if(filhos[0] == 0 || filhos[1] == 0) {
        // Area dos filhos
        signal(SIGTERM, childHandler);

        while(1) {
            printf("Dormindo por 1 segundo...\n");
            sleep(1);
        }
    }

    // Area do pai, pois os filhos nunca saem do while

    for(vezesContinuadas = 0; vezesContinuadas < 10; vezesContinuadas++) {
        printf("Faltam %d iteracoes.\n", 10 - vezesContinuadas);
        for (int i = 0; i < 2; i++) {
            kill(filhos[i], SIGCONT);
            sleep(2);
            kill(filhos[i], SIGSTOP);
        }


    }

    printf("Pai saiu do for.\n");

    // Após 10 vezes continuadas, mata os dois filhos.
    kill(filhos[1], SIGCONT);
    kill(filhos[0], SIGCONT);
    kill(filhos[0], SIGTERM);
    kill(filhos[1], SIGTERM);

    waitpid(filhos[0], NULL, 0);
    waitpid(filhos[1], NULL, 0);

    return 0;
}