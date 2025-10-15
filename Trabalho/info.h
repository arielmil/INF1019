#ifndef INFO_H
#define INFO_H

#include <sys/types.h> // Para tipos como pid_t

// Estados dos processos
#define PREEMPTED 0     // Interrompido por IRQ0 (fim do quantum)
#define WAITING_D1 1    // Esperando IRQ1 (D1)
#define WAITING_D2 2    // Esperando IRQ2 (D2)
#define RUNNING 3       // Em execução (após SIGCONT e antes do próximo SIGSTOP)
#define TERMINATED 4    // Finalizado

typedef struct info {
    int state;              // Estado atual
    char lastD;             // '1' para D1, '2' para D2, ou 0 se nenhum
    char lastOp;            // 'R','W','X' (última operação), ou 0 se nenhuma
    int PC;                 // Program Counter (interações concluídas)
    int timesD1Acessed;     // Quantas vezes acessou D1
    int timesD2Acessed;     // Quantas vezes acessou D2
    pid_t pid;              // PID do processo
} Info;

#endif
