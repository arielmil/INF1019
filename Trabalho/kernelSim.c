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

    perror("[KernelSim - getProcesssNumber]: Erro ao tentar encontrar o processNumber de um processo. Saindo...");
    exit(-21);
}

void irq0Handler(int signum) {
    lastSignal = SIG_IRQ0;
    return;
}

void irq1Handler(int signum) {
    lastSignal = SIG_IRQ1;
    return;
}

void irq2Handler(int signum) {
    lastSignal = SIG_IRQ2;
    return;
}

int main(void) {
    char bufferan1, bufferan2;

    char processNumber[2];
    char shmIdICSString[23];
    char shmIdProcessString[23];
    char pid_kernelSimString[23];

    int terminatedProcessess;
    int fifoan1, fifoan2, fifoics;
    int test;
    int i;
    int shmIdICS;

    int *shmICSptr;
    int shmIdProcess[5];

    pid_t ICS;
    pid_t pid_kernelSim;

    pid_t process[5];

    PD pd[5];

    Info *info[5];

    pid_kernelSim = getpid();

    if (mkfifo("FIFOAN1", (S_IRUSR | S_IWUSR)) != 0) {
        perror("[KernelSim]: Erro ao criar a FIFOAN1. Saindo...");
        exit(-5);
    } 

    if ((fifoan1 = open("FIFOAN1", (O_RDONLY | O_NONBLOCK))) < 0) {
        perror("[KernelSim]: Erro ao abrir a FIFOAN1 para leitura. Saindo...");
        exit(-6);
    }

    if (mkfifo("FIFOAN2", (S_IRUSR | S_IWUSR)) != 0) {
        perror("[KernelSim]: Erro ao criar a FIFOAN2. Saindo...");
        exit(-5);
    } 

    if ((fifoan2 = open("FIFOAN2", (O_RDONLY | O_NONBLOCK))) < 0) {
        perror("[KernelSim]: Erro ao abrir a FIFOAN2 para leitura. Saindo...");
        exit(-6);
    }

    if (mkfifo("FIFOCS", (S_IRUSR | S_IWUSR)) != 0) {
        perror("[KernelSim]: Erro ao criar a FIFOICS. Saindo...");
        exit(-5);
    } 

    if ((fifoics = open("FIFOICS", (O_RDONLY | O_NONBLOCK))) < 0) {
        perror("[KernelSim]: Erro ao abrir a FIFOICS para leitura. Saindo...");
        exit(-6);
    }

    // Cria shmem para passar para ICS as shmId da Info* de cada process
    shmIdICS = shmget(IPC_PRIVATE, sizeof(int) * 5, IPC_CREAT | IPC_EXCL | S_IRUSR | S_IWUSR); // Read-write pelo dono, cria se não existe, falha se existe.
    if (shmIdICS < 0) {
        perror("[KernelSim]: Erro ao criar região de memória compartilhada para ICS. Saindo...");
        exit(-28);
    }

    shmICSptr = (int *)shmat(shmIdICS, NULL, 0);
    if (shmICSptr == (void *) -1) {
        perror("[KernelSim]: Erro ao anexar região de memória compartilhada para ICS. Saindo...");
        exit(-30);
    }    

    ICS = fork();
    if (ICS < 0) {
        perror("[KernelSim]: Erro ao fazer o fork para o ICS. Saindo...");
        exit(-7);
    }

    if (ICS == 0) {
        // Area do filho ICS

        // Converte valores numericos em string
        snprintf(shmIdICSString, sizeof(shmIdICSString), "%ld", (long)shmIdICS);
        snprintf(pid_kernelSimString, sizeof(pid_kernelSimString), "%d", (int)pid_kernelSim);

        char *args[] = {"interruptionControllerSim", pid_kernelSimString, shmIdICSString, NULL};
        execvp("./interruptionControllerSim", args);
    }

    // Deixa ICS em estado parado até o escalonamento começar
    test = kill(ICS, SIGSTOP);
    if (test == -1) {
        perror("[KernelSim]: Erro ao enviar sinal SIGSTOP para ICS. Saindo...");
        exit(-26);
    }

    for(i = 0; i < 5; i++) {

        // Cria shmem para se comunicar com os 5 processos filho
        shmIdProcess[i] = shmget(IPC_PRIVATE, sizeof(Info), IPC_CREAT | IPC_EXCL | S_IRUSR | S_IWUSR); // Read-write pelo dono, cria se não existe, falha se existe.
        if (shmIdProcess[i] < 0) {
            perror("[KernelSim]: Erro ao criar região de memória compartilhada para um processo. Saindo...");
            exit(-8);
        }
        
        // Anexa o segmento de memória compartilhada
        info[i] = (Info *) shmat(shmIdProcess[i], NULL, 0);
        if (info[i] == (void *) -1) {
            perror("[KernelSim]: Erro ao anexar segmento de memória compartilhada para um processo. Saindo...");
            // Remove o segmento de memória compartilhada em caso de erro
            shmctl(shmIdProcess[i], IPC_RMID, NULL);
            exit(-20);
        }

        // Escreve na shmem de ICS qual o id de uma das shmem do processo Ai
        shmICSptr[i] = shmIdProcess[i];

        process[i] = fork();
        if(process[i] < 0) {
            perror("[KernelSim]: Erro ao fazer o fork para um processo. Saindo...");
            exit(-9 - i);
        }

        if (process[i] == 0) {
            // Area do filho processo i

            // Para converter processNumber (i) e shmId para string
            sprintf(processNumber, "%d", i + 1);
            snprintf(shmIdProcessString, sizeof(shmIdProcessString), "%d", (int)shmIdProcess[i]);

            char *args[] = {"process", shmIdProcessString, processNumber, NULL};
            execvp("./process", args);
        }

        // Começa o estado de cada processo AN como parado
        test = kill(process[i], SIGSTOP);
        if (test == -1) {
            perror("[KernelSim]: Erro ao mandar um SIGSTOP para algum AN na hora de criar-lo. Saindo...");
            exit(-17);
        }

        // Atribui a informação de pid em info[i] o pid de process[i]  
        info[i]->pid = process[i];

        // Monta uma das 5 entradas para o dicionario de PID para numero de processo [1...5]
        pd[i].pid = process[i];
        pd[i].processNumber = i + 1;
    }

    // A partir daqui apenas o pai roda, pos todos os filhos estão executando cada um seu código
    signal(SIG_IRQ0, irq0Handler);
    signal(SIG_IRQ1, irq1Handler);
    signal(SIG_IRQ2, irq2Handler);

    Fila ready;
    Fila waitingD1;
    Fila waitingD2;

    init(&ready);
    init(&waitingD1);
    init(&waitingD2);

    // Faz ICS voltar a continuar
    test = kill(ICS, SIGCONT);
    if (test == -1) {
        perror("[KernelSim]: Erro ao enviar sinal SIGSCONT para ICS. Saindo...");
        exit(-27);
    }

    // Coloca os 5 processos AN na fila de prontos
    for (i = 0; i < 5; i++) {
        push(&ready, process[i]);
    }

    // A partir daqui, escalona
    pid_t currentProcess;
    Info *currentInfo;
    int processNumberValue;


    /* 
        Fluxo de funcionamento:

            ICS manda um IRQ0 a cada 0.5s
            ICS tem 10% de chance de mandar um IRQ1 a cada 0.5s
            ICS tem 0.5% de chance de mandar um IRQ2 a cada 0.5s

            Caso um IRQ0 seja mandado:

                Simplesmente da um SIGSTOP para ele e o coloca na fila de prontos novamente (Caso PC < MAX)
                Se PC >= MAX, não faz nada com este processo (Não o coloca novamente na fila de prontos)
            
            Caso um IRQ1 seja mandado:

                Checa se a fila de waitingD1 está vazia

                Se estiver, não faz nada

                Se não estiver, da um pop em waitingD1, e coloca o processo na fila de prontos novamente (Caso PC < MAX)
                Se PC >= MAX, não faz nada com este processo (Não o coloca novamente na fila de prontos)

            Caso um IRQ2 seja mandado:

                Checa se a fila de waitingD2 está vazia

                Se estiver, não faz nada

                Se não estiver, da um pop em waitingD2, e coloca o processo na fila de prontos novamente (Caso PC < MAX)
                Se PC >= MAX, não faz nada com este processo (Não o coloca novamente na fila de prontos)
    */
    
    terminatedProcessess = 0;
    while(terminatedProcessess < 5) {
        currentProcess = pop(&ready);

        // Pega a região da shmem específica para o processo atual
        processNumberValue = getProcessNumber(currentProcess, pd);
        currentInfo = info[processNumberValue - 1];

        //Manda um sinal para o ICS para indicar o inicio da contagem de um dt de 0.5 segundos
        //ICS mandara um sinal IRQ0 em 0.5 segs sinalizando que currentProcess deve ser interrompido
        test = kill(ICS, SIGCONT);
        if (test == -1) {
            perror("[KernelSim]: Erro ao mandar um SIGCONT para o ICS. Saindo...");
            exit(-19);
        }

        // Troca o estado do processo atual para rodando
        currentInfo->state = RUNNING;

        // Continua um processo pronto (Primeiro da fila)
        test = kill(currentProcess, SIGCONT);
        if (test == -1) {
            perror("[KernelSim]: Erro ao mandar um SIGCONT para algum AN. Saindo...");
            exit(-18);
        }

        // Controle é desviado para currentProcess, então não precisa pausar.

        // Controle voltou para KS devido a uma systemcall de currentProcess de R, W ou X para D1 ou D2, ou a um IRQ0 vindo de ICS
        test = read(fifoan1, &bufferan1, 1);
        if (test < 0) {
            perror("[KernelSim]: Erro ao ler da FIFOAN1. Saindo...");
            exit(-22);
        }

        //Atualiza o contador de programas de currentProcess
        currentInfo->PC++;

        if (bufferan1 == '1' || bufferan1 == '2'){
            test = read(fifoan2, &bufferan2, 1);
            if (test <= 0) {
                perror("[KernelSim]: Erro ao ler da FIFOAN2. Saindo...");
                exit(-24);
            }

            if (bufferan1 == '1') {
                // Dispositivo 1 acessado
                currentInfo->timesD1Acessed++;
                currentInfo->state = SIG_IRQ1;

                // Coloca currentProcess na fila de espera para D1
                push(&waitingD1, currentProcess);
            }

            else {
                // Dispositivo 2 acessado
                currentInfo->timesD2Acessed++;
                currentInfo->state = SIG_IRQ2;

                // Coloca currentProcess na fila de espera para D2
                push(&waitingD2, currentProcess);
            }

            currentInfo->lastD = bufferan1;

            if (bufferan2 == 'R' || bufferan2 == 'W' || bufferan2 == 'X') {
                currentInfo->lastOp = bufferan2;

                // Faz syscall para ICS para iniciar contagem de IRC1 ou IRC2
                test = kill(ICS, SIGUSR1);
                if (test == -1) {
                    perror("[KernelSim]: Erro ao mandar um SIGUSR1 para o ICS. Saindo...");
                    exit(-31);
                }

            }

            else {
                perror("[KernelSim]: Erro: Opcao inexistente para bufferan2. Saindo...");
                exit(-24);
            }
        }

        else if (bufferan1 == EOF) {
            // currentProcess não escreveu em FIFOAN1, então não se faz nada
        }

        else {
            // Erro
            perror("[KernelSim]: Erro: Opcao inexistente para bufferan1. Saindo...");
            exit(-23);
        }


        
        if (currentInfo->PC >= MAX) {
            //Processo não volta a ser escalonado
            currentInfo->state = TERMINATED;
            terminatedProcessess++;
        }
        
        //Tratamentos de sinais de ICS

        // Controle voltou para KS devido a um IRQ gerado por IC
        if (lastSignal == SIG_IRQ0 || lastSignal == SIG_IRQ1 || lastSignal == SIG_IRQ2) {
            if (currentInfo->state != TERMINATED) {
                //Processo volta a ser escalonado pois currentProcess.PC < MAX
                
                if (lastSignal == SIG_IRQ0) {
                    test = kill(currentProcess, SIGSTOP);
                    if (test == -1) {
                        perror("[KernelSim]: Erro ao mandar um SIGSTOP para algum AN. Saindo...");
                        exit(-19);
                    }

                }

                else if (lastSignal == SIG_IRQ1) {
                    if (!empty(&waitingD1)) {
                        currentProcess = pop(&waitingD1);
                    }
                }

                else {
                    if (!empty(&waitingD2)) {
                        currentProcess = pop(&waitingD2);
                    }
                }

                // Atualiza currentInfo, pois pode se tratar de outro process
                currentInfo = info[processNumberValue - 1];

                if (currentInfo->state != TERMINATED) {
                    push(&ready, currentProcess);
                }
            }
            
        }

        else {
            perror("[KernelSim]: Erro: lastSignal enviado por ICS para KernelSim invalido. Saindo...");
            exit(-25);
        }

        //Coloca currentProcess novamente na fila de prontos, pois está pronto para ser posteriormente escalonado.
        

    }

    //Fecha tudo e sai

    //Deslinka das FIFOS 
    unlink("FIFOAN1");
    unlink("FIFOAN2");
    unlink("FIFOICS");

    // Fecha as FIFOS
    close(fifoan1);
    close(fifoan2);
    close(fifoics);

    // Dettacha e deleta as shm
    for (i = 0; i < 5; i++) {
        shmdt(info[i]);
        shmctl(shmIdProcess[i], IPC_RMID, NULL);
    }

    shmdt(shmICSptr);
    shmctl(shmIdICS, IPC_RMID, NULL);

    // Limpa as filas
    clear(&ready);
    clear(&waitingD1);
    clear(&waitingD2);

    return 0;
}