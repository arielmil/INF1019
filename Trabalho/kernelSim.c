#include <stdio.h>
#include <stdlib.h> // Para exit e NULL
#include <sys/shm.h> // Para shmget(), shmat(), shmdt() e shmctl()
#include <sys/ipc.h> // Para flags IPC_CREAT, IPC_EXCL, IPC_NOWAIT e estrutura ipc_perm
#include <sys/stat.h> // Para flags de permissão
#include <sys/types.h> // Para tipos como pid_t
#include <fcntl.h> // Para flags de controle de FIFO.
#include <signal.h> // Para tratamento de sinais
#include <sys/queue.h> // Para uso de filas
#include <unistd.h> // Para pause()

#include "info.h"
#include "irq.h"
#include "fila.c"

// Fazer em seguida:

/*

    Quando um process fizer alguma syscall simulada para D1 ou D2, enviar uma mensagem para kernelSim através de um SIGUSR1
    kernelSim então deve olhar para a shmem deste process para verificar qual tipo de operação será feita e para qual dispositivo.
    KernelSim deve então mandar uma mensagem (decidir a melhor forma) para ICS para sinalizar a operação, o dispositivo sendo requisitado, e qual processo (numero do processo e talvez pid) fez essa requisição
    
    O controle deve ser desviado então para ICS que deve tratar a interrupção adequadamente

    Fazer a parte de interação entre kernelSim e ICS para as interrupções de fatia de tempo (IRQ0)
    
    Passar para kernelSim através de um Pipe, todas as keys de shmem de cada processo para que ele possa exibir as informações pedidas quando interrompido com ctrl-c (Ver se precisa)

    Trocar a lógica de mexer na estrutura info no kernelSim, e o process faz o request,
    ao invés do process em sí mexer direto na estrutura, process apenas faz o request para KS fazer isso.

*/


int lastSignal = -1;

typedef struct processDictionary {
    pid_t pid;
    int processNumber
}PD;

int getProcessNumber(pid_t pid, PD *pd) {
    for (int i = 0; i < 5; i++) {
        PD current = pd[i];
        if (current.pid == pid) {
            return current.processNumber;
        }
    }

    perror("[kernelSim - getProcesssNumber]: Erro ao tentar encontrar o processNumber de um processo. Saindo...");
    exit(-21);
}

void irq0Handler(int signum) {
    lastSignal = IRQ0;
    return;
}

void irq1Handler(int signum) {
    lastSignal = IRQ1;
    return;
}

void irq2Handler(int signum) {
    lastSignal = IRQ2;
    return;
}

int main(void) {
    pid_t ICS;
    pid_t process[5];

    PD pd[5];

    char processNumber[2];
    char shmIdString[23];

    int terminatedProcessess;
    int shmId[5];
    int fifoan, fifocs;
    int test;
    int i;

    Info *info[5];

    if (mkfifo("FIFOAN", (S_IRUSR | S_IWUSR)) != 0) {
        perror("Erro ao criar a FIFOAN. Saindo...");
        exit(-5);
    } 

    if ((fifoan = open("FIFOAN", (O_RDONLY | O_NONBLOCK))) < 0) {
        perror("Erro ao abrir a FIFOAN para leitura. Saindo...");
        exit(-6);
    }

    if (mkfifo("FIFOCS", (S_IRUSR | S_IWUSR)) != 0) {
        perror("Erro ao criar a FIFOCS. Saindo...");
        exit(-5);
    } 

    if ((fifocs = open("FIFOCS", (O_RDONLY | O_NONBLOCK))) < 0) {
        perror("Erro ao abrir a FIFOCS para leitura. Saindo...");
        exit(-6);
    }

    ICS = fork();
    if (ICS < 0) {
        perror("Erro ao fazer o fork para o ICS. Saindo...");
        exit(-7);
    }

    if (ICS == 0) {
        // Area do filho ICS
        char *args[] = {"interruptionControllerSim", NULL};
        execvp("./interruptionControllerSim", args);
    }

    for(i = 0; i < 5; i++) {

        // Cria shmem para se comunicar com os 5 processos filho
        shmId[i] = shmget(IPC_PRIVATE, sizeof(Info), IPC_CREAT | IPC_EXCL | S_IRUSR | S_IWUSR); // Read-write pelo dono, cria se não existe, falha se existe.
        if (shmId[i] < 0) {
            perror("Erro ao criar região de memória compartilhada para um processo. Saindo...");
            exit(-8);
        }
        
        // Anexa o segmento de memória compartilhada
        info[i] = (Info *) shmat(shmId[i], NULL, 0);
        if (info[i] == (void *) -1) {
            perror("[KernelSim]: Erro ao anexar segmento de memória compartilhada. Saindo...");
            // Remove o segmento de memória compartilhada em caso de erro
            shmctl(shmId[i], IPC_RMID, NULL);
            exit(-20);
        }

        process[i] = fork();
        if(process[i] < 0) {
            perror("Erro ao fazer o fork para um processo. Saindo...");
            exit(-9 - i);
        }

        if (process[i] == 0) {
            // Area do filho processo i

            // Para converter processNumber (i) e shmId para string
            sprintf(processNumber, "%d", i + 1);
            snprintf(shmIdString, sizeof(shmIdString), "%ld", (long)shmId[i]);

            char *args[] = {"process",shmIdString, processNumber, NULL};
            execvp("./process", args);
        }

        // Começa o estado de cada processo AN como parado
        test = kill(process[i], SIGSTOP);
        if (test == -1) {
            perror("Erro ao mandar um SIGSTOP para algum AN na hora de criar-lo. Saindo...");
            exit(-17);
        }

        // Monta uma das 5 entradas para o dicionario de PID para numero de processo [1...5]
        pd[i].pid = process[i];
        pd[i].processNumber = i + 1;
    }

    // A partir daqui apenas o pai roda, pos todos os filhos estão executando cada um seu código
    signal(IRQ0, irq0Handler);
    signal(IRQ1, irq1Handler);
    signal(IRQ2, irq2Handler);

    Fila ready;
    Fila waitingD1;
    Fila waitingD2;

    init(&ready);
    init(&waitingD1);
    init(&waitingD2);

    // Coloca os 5 processos AN na fila de prontos
    for (i = 0; i < 5; i++) {
        push(&ready, process[i]);
    }

    // A partir daqui, escalona
    pid_t currentProcess;
    Info *currentInfo;
    int processNumberValue;

    while(terminatedProcessess < 5) {
        currentProcess = pop(&ready);

        // Pega a região da shmem específica para o processo atual
        processNumberValue = getProcessNumber(currentProcess, pd);
        currentInfo = info[processNumberValue];

        //Manda um sinal para o ICS para indicar o inicio da contagem de um dt de 0.5 segundos
        test = kill(ICS, SIGUSR1);
        if (test == -1) {
            perror("Erro ao mandar um SIGUSR1 para o ICS. Saindo...");
            exit(-19);
        }

        // Troca o estado do processo atual para rodando
        currentInfo->state = RUNNING;

        // Continua um processo pronto (Primeiro da fila)
        test = kill(currentProcess, SIGCONT);
        if (test == -1) {
            perror("Erro ao mandar um SIGCONT para algum AN. Saindo...");
            exit(-18);
        }

        // Controle é desviado para currentProcess, então não precisa pausar.

        // Controle voltou para KS devido a um IRQ0 gerado por IC
        if (lastSignal == IRQ0) {
            test = kill(currentProcess, SIGSTOP);
            if (test == -1) {
                perror("Erro ao mandar um SIGSTOP para algum AN. Saindo...");
                exit(-19);
            }

            
            if (currentInfo->PC >= MAX) {
                //Processo não volta a ser escalonado
                currentInfo->state = TERMINATED;
                terminatedProcessess++;
            }

            else {
                //Coloca currentProcess novamente na fila de prontos, pois está pronto para ser posteriormente escalonado.
                push(&ready, currentProcess);
            }
            
        }

    }

    //Fecha tudo e sai
}