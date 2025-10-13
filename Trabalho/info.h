#include <sys/types.h> // Para tipos como pid_t

typedef struct info {
    char state; // 0: Interrompido por IRQ0, 1: Esperando Por IRQ1, 2: Esperando por IRQ2, 3: Rodando, 4: Terminado
    char lastD; // D1 ou D2
	char lastOp; // Qual foi a última operação que este processo fez em D1 ou D2
    int PC; // Contador de Programa
    pid_t pid; //Qual o pid do processo
} Info;