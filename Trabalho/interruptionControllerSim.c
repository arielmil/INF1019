#include <stdio.h>
#include <stdlib.h>     // Para exit
#include <unistd.h>     // Para sleep, getpid
#include <signal.h>     // Para kill, signal
#include <time.h>       // Para srand, rand
#include <sys/ipc.h>    // Para shmget
#include <sys/shm.h>    // Para shmat, shmdt
#include <sys/types.h>  // Para pid_t

#include "irq.h"
#include "info.h"

// argv[1] = pid do KernelSim
// argv[2] = shmid da tabela Info[5]

static volatile sig_atomic_t snapshotFlag = 0; // CORREÇÃO: dispara UM snapshot por Ctrl-C

void sigint_handler(int signo) {
    snapshotFlag = 1;
}

int main(int argc, char *argv[]) {
    if (argc < 3) {
        fprintf(stderr, "[ICS]: uso: %s <pid-kernel> <shmid-info>\n", argv[0]);
        exit(-1);
    }

    pid_t ks_pid = (pid_t) atoi(argv[1]);
    int shmid = atoi(argv[2]);

    signal(SIGINT, sigint_handler); // CORREÇÃO: Ctrl-C gera snapshot único

    Info *info = (Info *) shmat(shmid, NULL, 0);
    if (info == (void *) -1) {
        perror("[ICS]: erro ao shmat tabela de estados");
        exit(-2);
    }

    srand((unsigned int) (getpid() ^ time(NULL)));

    while (1) {
        if (snapshotFlag == 1) {
            // CORREÇÃO: imprime uma vez, no formato do usuário, e espera scanf para continuar
            printf("[ICS]: Briefing do estado de todos os processos AN:\n");
            int i = 0;
            while (i < 5) {
                int state = info[i].state;
                char lastD = info[i].lastD;
                char lastOp = info[i].lastOp;
                int PC = info[i].PC;
                int timesD1Acessed = info[i].timesD1Acessed;
                int timesD2Acessed = info[i].timesD2Acessed;
                pid_t pid = info[i].pid;

                // CORREÇÃO: 'state' é numérico; imprimir como caractere '0'..'4' para manter o %c do seu formato
                char stateChar = (char) ('0' + (state & 0xF));

                printf("Processo %d de PID %d:\n"
                       "Estado: %c\n"
                       "Ultimo dispositivo acessado: %c\n"
                       "Ultima operacao feita no dispositivo: %c\n"
                       "PC: %d\n"
                       "Quantidade de vezes que acessou D1: %d\n"
                       "Quantidade de vezes que acessou D2: %d.\n\n",
                       i + 1, (int) pid, stateChar, lastD, lastOp, PC, timesD1Acessed, timesD2Acessed);

                i = i + 1;
            }

            printf("Digite qualquer coisa para continuar...\n");
            char c;
            scanf(" %c", &c);
            snapshotFlag = 0;

        } 
        
        else {
            // Geração de IRQs (mesma lógica)
            usleep(500000);
            kill(ks_pid, SIG_IRQ0);

            int p = (rand() % 1000) + 1;
            if (p < 100) {
                // Chance de 10%
                kill(ks_pid, SIG_IRQ1);
            }

            if (p < 5) {
                // Chance de 0.5%
                kill(ks_pid, SIG_IRQ2);
            }
        }
    }

    // Não chegamos aqui normalmente
    shmdt(info);
    return 0;
}
