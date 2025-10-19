#include <stdio.h>
#include <stdlib.h> // Para exit e NULL
#include <sys/shm.h> // Para shmget(), shmat(), shmdt() e shmctl()
#include <sys/ipc.h> // Para flags IPC_CREAT, IPC_EXCL, IPC_NOWAIT e estrutura ipc_perm
#include <sys/stat.h> // Para flags de permissão
#include <sys/types.h> // Para tipos como pid_t
#include <sys/wait.h> //Para waitpid()
#include <fcntl.h> // Para flags de controle de FIFO.
#include <signal.h> // Para tratamento de sinais
#include <unistd.h> // Para pause()
#include <errno.h>

#include "info.h"
#include "irq.h"
#include "fila.h"

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

static volatile sig_atomic_t irq0Pending = 0;
static volatile sig_atomic_t irq1Pending = 0;
static volatile sig_atomic_t irq2Pending = 0;

typedef struct processDictionary {
    pid_t pid;
    int processNumber;
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
    irq0Pending = 1;
    return;
}

void irq1Handler(int signum) {
    irq1Pending = 1;
    return;
}

void irq2Handler(int signum) {
    irq2Pending = 1;
    return;
}

int main(void) {
    char bufferD, bufferOp;
    char bufferan[5];

    char processNumber[2];
    char shmIdICSString[23];
    char shmIdProcessString[23];
    char pid_kernelSimString[23];

    int terminatedProcessess = 0;
    int fifoan;
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

    // Sinaliza para ignorar um SIGINT
    signal(SIGINT, SIG_IGN);

    //Deslinka as FIFO para garantir criação segura
    unlink("FIFOAN");

    if (mkfifo("FIFOAN", (S_IRUSR | S_IWUSR)) != 0) {
        perror("[KernelSim]: Erro ao criar a FIFOAN. Saindo...");
        exit(-5);
    } 

    if ((fifoan = open("FIFOAN", (O_RDONLY | O_NONBLOCK))) < 0) {
        perror("[KernelSim]: Erro ao abrir a FIFOAN para leitura. Saindo...");
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

            char *args[] = {"process", processNumber, shmIdProcessString, NULL};
            execvp("./process", args);

            // Nao é para voltar desse execvp
            _exit(127);
        }

        // Começa o estado de cada processo AN como parado
        test = kill(process[i], SIGSTOP);
        if (test == -1) {
            perror("[KernelSim]: Erro ao mandar um SIGSTOP para algum AN na hora de criar-lo. Saindo...");
            exit(-17);
        }

        // Atribui a informação de pid em info[i] o pid de process[i]  
        info[i]->pid = process[i];
        info[i]->state = STOPPED;
        info[i]->lastD = '-';
        info[i]->lastOp = '-';
        info[i]->PC = 0;
        info[i]->timesD1Acessed = 0;
        info[i]->timesD2Acessed = 0;

        // Monta uma das 5 entradas para o dicionario de PID para numero de processo [1...5]
        pd[i].pid = process[i];
        pd[i].processNumber = i + 1;
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

        // Nao é para voltar desse execvp
        _exit(127);
    }

    // Deixa ICS em estado parado até o escalonamento começar
    test = kill(ICS, SIGSTOP);
    if (test == -1) {
        perror("[KernelSim]: Erro ao enviar sinal SIGSTOP para ICS. Saindo...");
        exit(-26);
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
    pid_t syscalledProcess;

    Info *currentInfo;
    Info *syscalledInfo;

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
    
    while(terminatedProcessess < 5) {
        
        while(empty(&ready)) {
            // Dorme por 0.1s para evitar espera ocupada e dar chance de outro processo ser escalonado
            usleep(100000);

            // Se recebeu um IRQ1
            if(irq1Pending) {

                if (!empty(&waitingD1)) {
                    syscalledProcess = pop(&waitingD1);
                    syscalledInfo = info[getProcessNumber(syscalledProcess, pd) - 1];
                    syscalledInfo->state = READY;
                    push(&ready, syscalledProcess);
                }

                // Coloca o sinal como tratado
                irq1Pending = 0;
            }

            if (irq2Pending) {

                if (!empty(&waitingD2)) {
                    syscalledProcess = pop(&waitingD2);
                    syscalledInfo = info[getProcessNumber(syscalledProcess, pd) - 1];
                    syscalledInfo->state = READY;
                    push(&ready, syscalledProcess);
                }

                // Coloca o sinal como tratado
                irq2Pending = 0;
            }
        }

        // A partir daqui é garantido que ready não está vazia
        currentProcess = pop(&ready);
        currentInfo = info[getProcessNumber(currentProcess, pd) - 1];
        
        currentInfo->state = RUNNING;

        test = kill(currentProcess, SIGCONT);
        if (test == -1) {
            perror("[KernelSim]: Erro ao enviar um SIGCONT para um processo. Saindo...");
            exit(-36);
        }

        // Continuan o while enquanto não recebe um IRQX, ou um syscall de process
        while(!irq0Pending && !irq1Pending && !irq2Pending) {
            // Dorme por 0.1s para evitar espera ocupada e dar chance de outro processo ser escalonado
            usleep(100000);

            errno = 0;
            test = read(fifoan, bufferan, 5);
            
            if (test <= 0) {
                if (errno == EAGAIN || errno == EWOULDBLOCK || test == 0) {
                    
                    // Nenhum syscall ou IRQX recebido
                    continue;
                }

                // else:
                perror("[KernelSim]: Erro ao tentar dar read para FIFOAN. Saindo...");
                exit(-37);
            }

            // else:
            bufferD = bufferan[0];
            bufferOp = bufferan[3];

            if (bufferD == '1') {
                push(&waitingD1, currentProcess);

                currentInfo->state = WAITING_D1;
                currentInfo->lastD = bufferD;
                currentInfo->lastOp = bufferOp;
                currentInfo->timesD1Acessed++;

                test = kill(currentProcess, SIGSTOP);
                if (test == -1) {
                    perror("[KernelSim]: Erro ao enviar um SIGSTOP para um processo. Saindo...");
                    exit(-41);
                }

            }

            else if (bufferD == '2') {
                push(&waitingD2, currentProcess);

                currentInfo->state = WAITING_D2;
                currentInfo->lastD = bufferD;
                currentInfo->lastOp = bufferOp;
                currentInfo->timesD2Acessed++;

                test = kill(currentProcess, SIGSTOP);
                if (test == -1) {
                    perror("[KernelSim]: Erro ao enviar um SIGSTOP para um processo. Saindo...");
                    exit(-39);
                }
            }

            else {
                perror("[KernelSim]: Erro: Opção invalida para dispositivo. Saindo...");
                exit(-38);
            }

            // Sai do while pois recebeu um syscall e precisa escalonar outro processo
            break;
        }

        if (currentInfo->PC >= MAX) {
            currentInfo->state = TERMINATED;
            terminatedProcessess++;
        }

        // Saiu do while por ter recebido um irq0
        if (irq0Pending) {
            if (currentInfo->state == RUNNING) {
                
                test = kill(currentProcess, SIGSTOP);
                if (test == -1) {
                    perror("[KernelSim]: Erro ao enviar um SIGSTOP para um processo. Saindo...");
                    exit(-40);
                }

                currentInfo->state = STOPPED;
            }

            irq0Pending = 0;
        }

        // Saiu do while por ter recebido um irq1
        if(irq1Pending) {
            if (!empty(&waitingD1)) {
                syscalledProcess = pop(&waitingD1);
                syscalledInfo = info[getProcessNumber(syscalledProcess, pd) - 1];
                syscalledInfo->state = READY;
                push(&ready, syscalledProcess);
            }

            irq1Pending = 0;
        }

        // Saiu do while por ter recebido um irq2
        if (irq2Pending) {
            if (!empty(&waitingD2)) {
                    syscalledProcess = pop(&waitingD2);
                    syscalledInfo = info[getProcessNumber(syscalledProcess, pd) - 1];
                    syscalledInfo->state = READY;
                    push(&ready, syscalledProcess);
                }

                irq2Pending = 0;
            }

        // Processo ainda não foi recolocado na fila de prontos (Pode juntar com a condição do IRQ0?)
        if (currentInfo->state != TERMINATED && currentInfo->state == STOPPED) {
            push(&ready, currentProcess);
            currentInfo->state = READY;
        }
    }
    //Fecha tudo e sai

    // Fecha a e unlinka a FIFO
    close(fifoan);

    unlink("FIFOAN");

    // Dettacha e deleta as shm
    for (i = 0; i < 5; i++) {
        shmdt(info[i]);
        shmctl(shmIdProcess[i], IPC_RMID, NULL);
    }

    shmdt(shmICSptr);
    shmctl(shmIdICS, IPC_RMID, NULL);

    // Encerra os 5 processos AN
    for(i = 0; i < 5; i++) {
        test = kill(process[i], SIGTERM);
        if (test == -1) {
            perror("[KernelSim]: Erro ao tentar encerrar algum processo. Saindo...");
            exit(-34);
        }

        // Para evitar filho zumbi
        waitpid(process[i], NULL, 0);
    }

    // Encerra o ICS
    
    test = kill(ICS, SIGTERM);
    if (test == -1) {
        perror("[KernelSim]: Erro ao tentar encerrar ICS. Saindo...");
        exit(-35);
    }

    // Para evitar filho zumbi
    waitpid(ICS, NULL, 0);


    // Limpa as filas
    clear(&ready);
    clear(&waitingD1);
    clear(&waitingD2);

    return 0;
}