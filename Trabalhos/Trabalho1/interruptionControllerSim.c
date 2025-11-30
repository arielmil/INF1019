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
int looping = 1;
static volatile sig_atomic_t wantBriefing = 0;

static const char *stateToString(char state) {
    switch (state) {
        case STOPPED:   return "PRONTO";
        case WAITING_D1:  return "ESPERANDO D1";
        case WAITING_D2:  return "ESPERANDO D2";
        case RUNNING:     return "RODANDO";
        case TERMINATED:  return "TERMINADO";
        case READY:       return "PRONTO";
        default:          return "DESCONHECIDO";
    }
}

static char normalizeDevice(char d) {
    if (d == '1' || d == '2') {
        return d;
    }

    else {
        return '-';
    }

    //return (d == '1' || d == '2') ? d : '-';
}

static char normalizeOp(char op) {
    if (op == 'R' || op == 'W' || op == 'X') {
        return op;
    }

    else {
        return '-';
    }

    //return (op == 'R' || op == 'W' || op == 'X') ? op : '-';
}

void termHandler(int signum) {
    looping = 0;
}

void interruptHandler(int signum) {
    wantBriefing = 1;
}

void doBriefing(void) {
    int i;

    printf("[ICS]: Briefing do estado de todos os processos AN:\n");
    for (i = 0; i < 5; i++) {
        char state          = info[i]->state;
        char lastD          = info[i]->lastD;
        char lastOp         = info[i]->lastOp;
        int  PC             = info[i]->PC;
        int  timesD1Acessed = info[i]->timesD1Acessed;
        int  timesD2Acessed = info[i]->timesD2Acessed;
        pid_t pid           = info[i]->pid;

        printf("Processo %d de PID %ld:\n"
               "Estado: %s\n"
               "Ultimo dispositivo acessado: %c\n"
               "Ultima operacao feita no dispositivo: %c\n"
               "PC: %d\n"
               "Quantidade de vezes que acessou D1: %d\n"
               "Quantidade de vezes que acessou D2: %d.\n\n",
               i + 1, (long)pid,
               stateToString(state),
               normalizeDevice(lastD),
               normalizeOp(lastOp),
               PC, timesD1Acessed, timesD2Acessed);
    }
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
        if (info[i] == (void *) -1) {
            perror("[ICS]: Erro au usar shmat para um info. Saindo...");
            _exit(-37);
        }
    }

    signal(SIGINT, interruptHandler);
    signal(SIGTERM, termHandler);

    srand((unsigned)(getpid() ^ time(NULL))); // Para seedar a função rand()

    while(looping) {
        usleep(500000);
        
        if(wantBriefing) {
            doBriefing();
            wantBriefing = 0;
        }

        kill(ks_pid, IRQ0);

        d = (rand()%1000) +1;

        if (d <= 100) {

            // Chance de 0.5% de ocorrencia
            if (d <= 5) {
                kill(ks_pid, IRQ2);
            }

            // Chance de 10% de ocorrencia
            else {
                // Enviar SIGUSR1 para desvia o fluxo para cá
                kill(ks_pid, IRQ1);
            }
        }
    }

    // Fecha tudo e sai
    shmdt(shmICSptr);

    for(i = 0; i < 5; i++) {
        shmdt(info[i]);
    }

    _exit(0);
}