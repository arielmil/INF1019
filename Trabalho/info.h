#include <sys/types.h> // Para tipos como pid_t

#define MAX 30

#define PREEMPTED 0
#define WAITING_D1 1
#define WAITING_D2 2
#define RUNNING 3
#define TERMINATED 4

typedef struct info {
    char state; // 0: Interrompido por IRQ0, 1: Esperando Por IRQ1, 2: Esperando por IRQ2, 3: Rodando, 4: Terminado
    char lastD; // 1 para D1 ou 2 para D2
	char lastOp; // Qual foi a última operação que este processo fez em D1 ou D2
    int PC; // Contador de Programa
    int timesD1Acessed; // Quantas vezes D1 foi acessado por processo com esta pid
    int timesD2Acessed; // Quantas vezes D2 foi acessado por processo com esta pid
    pid_t pid; //Qual o pid do processo
} Info;