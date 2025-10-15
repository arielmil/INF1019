#include <unistd.h> // Para getpid() e usleep()
#include <stdlib.h> // Para NULL e _exit
#include <time.h> // Para time()
#include <sys/shm.h> // Para shmat()
#include <sys/types.h> // Para tipos como pid_t
#include <signal.h> // Para tratamento de sinais
#include <stdio.h>
#include "info.h"
#include "irq.h"

Info *info[5];

void sigusr1Hanlder(int signum) {
    // Feito apenas para desviar o fluxo novamente para ICS.
    int a = 1;
    return;
}

void interruptHandler(int signum) {
    // Faz o briefing
    char c;

    char state; // 0: Interrompido por IRQ0, 1: Esperando Por IRQ1, 2: Esperando por IRQ2, 3: Rodando, 4: Terminado
    char lastD; // 1 para D1 ou 2 para D2
	char lastOp; // Qual foi a última operação que este processo fez em D1 ou D2
    int PC; // Contador de Programa
    int timesD1Acessed; // Quantas vezes D1 foi acessado por processo com esta pid
    int timesD2Acessed; // Quantas vezes D2 foi acessado por processo com esta pid
    pid_t pid; //Qual o pid do processo

    printf("[ICS]: Briefing do estado de todos os processos AN:\n");
    for (int i = 0; i < 5; i++) {

        state           = info[i]->state;
        lastD           = info[i]->lastD;
        lastOp          = info[i]->lastOp;
        PC              = info[i]->PC;
        timesD1Acessed  = info[i]->timesD1Acessed;
        timesD2Acessed  = info[i]->timesD2Acessed;
        pid             = info[i]->pid;

        printf("Processo %d de PID %d:\n"
               "Estado: %c\n"
               "Ultimo dispositivo acessado: %c\n"
               "Ultima operacao feita no dispositvo: %c\n"
               "PC: %d\n"
               "Quantidade de vezes que acessou D1: %d\n"
               "Quantidade de vezes que acessou D2: %d.\n\n",
               i + 1, (int)pid, state, lastD, lastOp, PC, timesD1Acessed, timesD2Acessed);
    }

    printf("Digite qualquer coisa para continuar...\n");
    scanf("%c", &c);
    return;
}

//argv[1]: pid de kernelSim, argv[2]: shmIdICS
int main(int argc, char *argv[]) {
    int shmIdICS;
    int i;
    int d;
    int *shmICSptr;

    pid_t ks_pid;
    
    if (argc < 3) {
        perror("[ICS]: Erro: ICS não recebeu o número de argumentos necessários. Saindo...\n");
        _exit(-29);
    }

    ks_pid = (pid_t) atoi(argv[1]);
    shmIdICS = atoi(argv[2]);

    shmICSptr = (int*) shmat(shmIdICS, NULL, 0);
    if (shmICSptr == (void *) -1) {
        perror("[ICS]: Erro ao usar shmat para shmICSptr. Saindo...");
        _exit(-31);
    }

    // Pega todos os Infos de cada processo para fazer o briefing caso seja interrompido por Ctrl-C
    for (i = 0; i < 5; i++) {
        info[i] = (Info *)shmat(shmICSptr[i], NULL, 0);
    }

    signal(SIGINT, interruptHandler);
    signal(SIGUSR1, sigusr1Hanlder);
    // CORREÇÃO: removido segundo signal(SIGINT, ...) que sobrescrevia o interruptHandler
    // signal(SIGINT, sigusr1Hanlder);

    srand((unsigned)(getpid() ^ time(NULL))); // Para seedar a função rand()

    while(1) {
        usleep(500000);
        
        // Enviar SIGUSR1 para desvia o fluxo para cá
        kill(ks_pid, SIG_IRQ0);

        d = (rand()%1000) +1;

        if (d <= 100) {

            // Chance de 0.5% de ocorrencia
            if (d <= 5) {
                // Enviar SIGUSR1 para desvia o fluxo para cá
                kill(ks_pid, SIG_IRQ2);
            }

            // Chance de 10% de ocorrencia
            else {
                // Enviar SIGUSR1 para desvia o fluxo para cá
                kill(ks_pid, SIG_IRQ1);
            }
        }
    }
}