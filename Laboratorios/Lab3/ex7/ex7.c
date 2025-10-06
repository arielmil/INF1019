#include <stdio.h>
#include <stdlib.h> // Para exit e _exit
#include <signal.h> // Para kill() e signal()
#include <sys/types.h> // Para tipos como pid_t
#include <unistd.h> // Para fork(), execve() e sleep()
#include <sys/wait.h> // Para waitpid()

pid_t filhos[3];

void rotinaFilho(int numFilho) {
    char arg2[2];

    sprintf(arg2, "%d", numFilho);
    char *argv[] = {"./filho", arg2, NULL};

    printf("Chamando o programa ./filho com argumento: numFilho = %s\n", arg2);

    execv("./filho", argv);
}

void fatherHandler(int signo) {
    printf("Pai encerrou o programa. Matando os filhos...");

    for (int i = 0; i < 3; i++) {
        kill(filhos[i], SIGCONT);
        kill(filhos[i], SIGKILL);

        // Para evitar filho zumbi
        waitpid(filhos[i], NULL, 0);
    }

    exit(0);
}

int main(void) {
    int i;

    for (i = 0; i < 3; i++) {
        filhos[i] = fork();

        if (filhos[i] < 0) {
            perror("Erro ao criar filho. Saindo...");
            exit(-1);
        }

        if (filhos[i] != 0) {

            // Para garantir começo sincronizado
            kill(filhos[i], SIGSTOP);
        }

        else {
            // Area dos filhos
            rotinaFilho(i);
        }
    }

    printf("Pai saiu do for.\n");

    // Filhos nunca voltam de execv, então essa é a area do pai
    signal(SIGINT, fatherHandler);

    while(1) {
        int sleepTime;

        for (i = 0; i < 3; i++) {
            if (i == 0) {
                sleepTime = 1;
            }

            else {
                sleepTime = 2;
            }

            kill(filhos[i], SIGCONT);

            sleep(sleepTime);

            kill(filhos[i], SIGSTOP);
        }
    }    

    return 0;
}