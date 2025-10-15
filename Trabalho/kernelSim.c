#include <stdio.h>
#include <stdlib.h>       // Para exit
#include <unistd.h>       // Para fork, execvp, sleep, pause
#include <string.h>       // Para memset, strerror
#include <errno.h>        // Para errno
#include <signal.h>       // Para sinais
#include <fcntl.h>        // Para open
#include <sys/stat.h>     // Para mkfifo
#include <sys/wait.h>     // Para waitpid
#include <sys/ipc.h>      // Para shmget
#include <sys/shm.h>      // Para shmat, shmdt, shmctl
#include <sys/types.h>    // Para pid_t

#include "irq.h"
#include "info.h"
#include "fila.h"

#define FIFOAN1 "FIFOAN1"
#define FIFOAN2 "FIFOAN2"

typedef struct processDictionary {
    pid_t pid;
    int processNumber; // CORREÇÃO: faltava ';' ao final do campo
} PD; // CORREÇÃO: typedef fechado corretamente

static volatile sig_atomic_t lastIRQ = -1;

void irq_handler0(int signo) {
    lastIRQ = 0;
}

void irq_handler1(int signo) {
    lastIRQ = 1;
}

void irq_handler2(int signo) {
    lastIRQ = 2;
}

// Utilitário simples: mapeia pid -> número (1..5)
int getProcessNumber(pid_t pid, PD pd[5]) {
    int i = 0;
    while (i < 5) {
        if (pd[i].pid == pid) {
            return pd[i].processNumber;
        } 
        
        i = i + 1;
    }

    return -1;
}

int main(void) {
    // Instala handlers das IRQs
    signal(SIG_IRQ0, irq_handler0);
    signal(SIG_IRQ1, irq_handler1);
    signal(SIG_IRQ2, irq_handler2);

    // Cria shmem da tabela Info[5] (para ICS ler)
    int shmid = shmget(IPC_PRIVATE, sizeof(Info) * 5, IPC_CREAT | 0600);
    if (shmid < 0) {
        perror("[KernelSim]: erro em shmget");
        exit(-1);
    }

    Info *info = (Info *) shmat(shmid, NULL, 0);
    if (info == (void *) -1) {
        perror("[KernelSim]: erro em shmat");
        exit(-2);
    }

    // Inicializa Info
    int i = 0;
    while (i < 5) {
        info[i].state = PREEMPTED;
        info[i].lastD = 0;
        info[i].lastOp = 0;
        info[i].PC = 0;
        info[i].timesD1Acessed = 0;
        info[i].timesD2Acessed = 0;
        info[i].pid = 0;
        i = i + 1;
    }

    // FIFOs para syscalls (processos <--> Kernel)
    unlink(FIFOAN1);
    unlink(FIFOAN2);

    if (mkfifo(FIFOAN1, 0600) < 0) {
        perror("[KernelSim]: erro ao criar FIFOAN1");
        exit(-3);
    }

    if (mkfifo(FIFOAN2, 0600) < 0) {
        perror("[KernelSim]: erro ao criar FIFOAN2");
        exit(-4);
    }

    int fifoan1 = open(FIFOAN1, O_RDONLY | O_NONBLOCK);
    if (fifoan1 < 0) {
        perror("[KernelSim]: erro ao abrir FIFOAN1 para leitura");
        exit(-5);
    }

    int fifoan2 = open(FIFOAN2, O_RDONLY | O_NONBLOCK);
    if (fifoan2 < 0) {
        perror("[KernelSim]: erro ao abrir FIFOAN2 para leitura");
        exit(-6);
    }

    // Cria ICS
    pid_t ics = fork();
    if (ics < 0) {
        perror("[KernelSim]: erro ao criar ICS");
        exit(-7);
    } 
    
    else if (ics == 0) {
        char ksPidStr[32];
        char shmidStr[32];

        sprintf(ksPidStr, "%d", (int) getppid());
        sprintf(shmidStr, "%d", shmid);

        char *args[] = {"interruptionControllerSim", ksPidStr, shmidStr, NULL};

        execvp("./interruptionControllerSim", args);

        perror("[KernelSim]: execvp ICS falhou");
        _exit(-8);
    }

    // Cria 5 processos de aplicação
    PD pd[5];
    Fila ready;
    Fila waitingD1;
    Fila waitingD2;

    init(&ready);
    init(&waitingD1);
    init(&waitingD2);

    i = 0;
    while (i < 5) {
        pid_t p = fork();
        if (p < 0) {
            perror("[KernelSim]: erro ao criar processo de aplicação");
            exit(-9);
        } 
        
        else if (p == 0) {
            char num[8];
            sprintf(num, "%d", i + 1);
            char *args[] = {"process", num, NULL};
            execvp("./process", args);
            perror("[KernelSim]: execvp process falhou");
            _exit(-10);
        } 
        
        else {
            pd[i].pid = p;
            pd[i].processNumber = i + 1; // 1..5
            info[i].pid = p;
            push(&ready, p);
        }
        i = i + 1;
    }

    // Dá um pequeno tempo para tudo existir
    sleep(1);

    // Loop de escalonamento
    pid_t current = -1;

    while (1) {
        // Se não há processo corrente, pegue o próximo pronto
        if (current == -1) {
            if (!empty(&ready)) {
                current = pop(&ready);
                int idx = getProcessNumber(current, pd) - 1;
                info[idx].state = RUNNING;
                kill(current, SIGCONT);
                lastIRQ = -1; // zera antes do quantum dt
            } 
            
            else {
                // Nada para rodar: verifique se todos terminaram
                int alive = 0;
                i = 0;
                while (i < 5) {
                    if (info[i].state != TERMINATED) {
                        alive = 1;
                    }
                    i = i + 1;
                }
                if (alive == 0) {
                    break;
                } 
                
                else {
                    usleep(20000);
                }
            }
        }

        // Consumir possíveis syscalls vindas das FIFOs (non-blocking)
        char d = 0;
        char op = 0;

        int r1 = read(fifoan1, &d, 1);
        if (r1 == -1) {
            if (errno == EAGAIN) {
                // Sem dados, segue
            } 
            
            else {
                perror("[KernelSim]: erro ao ler FIFOAN1");
            }
        }

        int r2 = read(fifoan2, &op, 1);
        if (r2 == -1) {
            if (errno == EAGAIN) {
                // sem dados, segue
            } 
            
            else {
                perror("[KernelSim]: erro ao ler FIFOAN2");
            }
        }

        // Se veio pedido completo (um byte em cada FIFO), faça preempção por syscall
        if (r1 == 1 && r2 == 1) {
            int idx = getProcessNumber(current, pd) - 1;
            if (idx >= 0) {
                info[idx].lastD = d;
                info[idx].lastOp = op;
                if (d == '1') {
                    info[idx].state = WAITING_D1; // CORREÇÃO: estado numérico coerente
                    info[idx].timesD1Acessed = info[idx].timesD1Acessed + 1;
                    push(&waitingD1, current);
                } 
                
                else if (d == '2') {
                    info[idx].state = WAITING_D2;
                    info[idx].timesD2Acessed = info[idx].timesD2Acessed + 1;
                    push(&waitingD2, current);
                } 
                
                else {
                    // Dispositivo inválido: ignore, mantém processo rodando
                }
            }

            // CORREÇÃO: preempção imediata em syscall
            kill(current, SIGSTOP);
            current = -1;
            continue;
        }

        // Espera até chegar uma IRQ (quantum ou desbloqueio)
        pause();

        // Tratamento da IRQ recebida
        if (lastIRQ == 0) {
            // IRQ0: fim do quantum do processo corrente
            if (current != -1) {
                int idx = getProcessNumber(current, pd) - 1;
                
                if (idx >= 0) {
                    info[idx].PC = info[idx].PC + 1; // CORREÇÃO: PC++ no fim do quantum
                    
                    if (info[idx].PC >= 25) {
                        info[idx].state = TERMINATED;
                        kill(current, SIGSTOP);
                        current = -1;
                    } 
                    
                    else {
                        info[idx].state = PREEMPTED;
                        kill(current, SIGSTOP);
                        push(&ready, current);
                        current = -1;
                    }
                } 
                
                else {
                    // PID não encontrado: para por segurança
                    kill(current, SIGSTOP);
                    current = -1;
                }
            }
        } 
        
        else if (lastIRQ == 1) {
            // IRQ1: libera primeiro da fila de D1 para pronto
            if (!empty(&waitingD1)) {
                pid_t p = pop(&waitingD1);
                int idx = getProcessNumber(p, pd) - 1;
                
                if (idx >= 0 && info[idx].state == WAITING_D1) {
                    info[idx].state = PREEMPTED;
                    push(&ready, p);
                }
            }

        } 
        
        else if (lastIRQ == 2) {
            // IRQ2: libera primeiro da fila de D2 para pronto
            
            if (!empty(&waitingD2)) {
                pid_t p = pop(&waitingD2);
                int idx = getProcessNumber(p, pd) - 1;
                
                if (idx >= 0 && info[idx].state == WAITING_D2) {
                    info[idx].state = PREEMPTED;
                    push(&ready, p);
                }
            }
        } 
        
        else {
            // IRQ desconhecida (ignorar)
        }

        // CORREÇÃO: zera a última IRQ processada
        lastIRQ = -1;
    }

    // Finalização: parar ICS e filhos remanescentes
    kill(ics, SIGTERM);

    i = 0;
    while (i < 5) {
        if (info[i].state != TERMINATED && info[i].pid != 0) {
            kill(info[i].pid, SIGTERM);
        }
        i = i + 1;
    }

    // Espera pelos filhos
    while (waitpid(-1, NULL, 0) > 0) {
        // nada
    }

    // Limpeza
    close(fifoan1);
    close(fifoan2);
    unlink(FIFOAN1);
    unlink(FIFOAN2);

    shmdt(info);
    shmctl(shmid, IPC_RMID, NULL);

    clear(&ready);
    clear(&waitingD1);
    clear(&waitingD2);

    return 0;
}
