#include <stdio.h>
#include <stdlib.h> // Para exit e NULL
#include <sys/shm.h> // Para shmget(), shmat(), shmdt() e shmctl()
#include <unistd.h> // Para getpid() e usleep()
#include <string.h> // Para strcpy() e strcmp()
#include <time.h> // Para time()
#include <fcntl.h> // Para flags de controle de FIFO.

#include "info.h"

// OBS: argv[0]: ./process, argv[1]: shmId, argv[2]: processNumber
int main(int argc, char *argv[]) {
    int PC = 0;
    char D, Op;
    int d;
    int fifo;
    int shmId;
    int processNumber; // [1..5]
    char *mensagem;
    
    if (argc < 3) {
        perror("Erro: Processo não recebeu o número de argumentos necessários. Saindo...\n");
        exit(-1);
    }

    shmId = atoi(argv[1]); // Pega o shmemId do kernel que criou uma struct em shmem para cada um dos 5 processos.
    processNumber = atoi(argv[2]);

    Info *info = (Info *) shmat(shmId, NULL, 0);
    if (info == (void *) -1) {
        perror("Erro ao anexar segmento de memória compartilhada. Saindo...\n");
        // Desanexa o segmento de memória compartilhada em caso de erro
        exit(-2);
    }

    fifo = open("FIFOAN", O_WRONLY); // Abre uma FIFO em modo de escrita bloqueante para sinalizar ao kernel a espera de IRQ1 e IRQ2
    if (fifo < 0) {
        perror("Erro ao abrir a FIFO 'FIFOAN' para escrita. Saindo...\n");
        shmdt(info);
        exit(-3);
    }

    char buffer[] = "Processo N esperando por um IRQX";
    mensagem = (char *)malloc(sizeof(char) * sizeof(buffer));
    if (mensagem == NULL) {
        perror("Erro ao fazer malloc. Saindo...");
        exit(-4);
    }

    strcpy(mensagem, buffer);

    info->lastOp = 0; // Nenhuma operação feita ainda
    info->lastD = 0; // Nenhum D foi usado ainda
    info->PC = 0;
    info->pid = getpid();

    srand((unsigned)(info->pid ^ time(NULL))); // Para seedar a função rand() 

    while (PC < MAX) {
        usleep(500000);
        
        d = rand()%100 +1;
        printf("Valor de d: %d em AN: %ld\n", d, (long) info->pid);

        if (d < 15) { // generate a random syscall
            if (d % 2 == 0) {
                D = '1';
            }

            else  {
                D = '2';
            }

            info->lastD = D;

            if (d % 3 == 0) {
                Op = 'R';
            }

            else if (d % 3 == 1) {
                Op = 'W';
            }

            else {
                Op = 'X';
            }

            info->lastOp = Op;

            mensagem[9] = processNumber + '0';
            mensagem[31] = D;

            write(fifo, mensagem, strlen(mensagem) + 1);
        }

        PC++;
        info->PC = PC;

        //Colocar mensagem de print

        usleep(500000);
    }

    free(mensagem);
    close(fifo);
    shmdt(info);

    return 0;
}