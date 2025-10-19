#include <stdio.h>
#include <stdlib.h> // Para exit e NULL
#include <unistd.h> // Para getpid() e usleep()
#include <time.h> // Para time()
#include <fcntl.h> // Para flags de controle de FIFO.
#include <signal.h> // Para signal()
#include <string.h> // Para strcpy()
#include <sys/shm.h> // Para shmget(), shmat(), shmdt() e shmctl()
#include "info.h"

// OBS: argv[0]: ./process, argv[1]: processNumber, argv[2]: shmIdProcess
int main(int argc, char *argv[]) {
    char D, Op;
    char buffer[] = "D, O";
    char *mensagem;
    int d;
    int fifoan;
    int shmIdProcess;
    int processNumber; // [1..5]
    int test;

    Info *info;

    pid_t pid = getpid();

    // Sinaliza para ignorar um SIGINT
    signal(SIGINT, SIG_IGN);

    // Caso o Kernel feche a FIFO antes do processo encerrar  
    signal(SIGPIPE, SIG_IGN);
    
    if (argc < 3) {
        perror("Erro: Processo não recebeu o número de argumentos necessários. Saindo...\n");
        _exit(-1);
    }

    processNumber = atoi(argv[1]);

    fifoan = open("FIFOAN", O_WRONLY); // Abre uma FIFO em modo de escrita bloqueante para sinalizar ao kernel a espera de IRQ0
    if (fifoan < 0) {
        perror("Erro ao abrir FIFOAN para escrita. Saindo...\n");
        _exit(-2);
    }

    shmIdProcess = atoi(argv[2]);
    info = shmat(shmIdProcess, NULL, 0);
    if (info == (void *) -1) {
        perror("[Process]: Erro ao usar shmat. Saindo...");
        _exit(-41);
    }

    srand((unsigned)(pid ^ time(NULL))); // Para seedar a função rand()

    mensagem = (char *)malloc(sizeof(char) * 5);
    if (mensagem == NULL) {
        perror("[Process]: Erro ao usar malloc para mensagem. Saindo...");
        _exit(-40);
    }

    strcpy(mensagem, buffer);

    while (info->PC < MAX) {
        usleep(500000);
        
        d = (rand()%100) +1;
        printf("Valor de d: %d em A%d (pid: %ld)\n", d, processNumber, (long) pid);

        if (d < 15) { // generate a random syscall
            // Prob de ocorrencia de 15%

            if (d % 2 == 0) {
                D = '1';
            }

            else  {
                D = '2';
            }

            if (d % 3 == 0) {
                Op = 'R';
            }

            else if (d % 3 == 1) {
                Op = 'W';
            }

            else {
                Op = 'X';
            }

            // Escreve em FIFO1 o disp usado e em FIFO2 qual a op para simular syscall
            // Isso deve desviar o fluxo de controle para KS
            printf("[Processo %d]: Dispositivo acessado: %c com operacao: %c.\n", processNumber, D, Op);

            mensagem[0] = D;
            mensagem[3] = Op;

            test = write(fifoan, mensagem, 5);
            if (test != 5) {
                perror("[Process]: Erro ao usar write. Saindo...");
                _exit(-41);
            }
        }

        else {
            // Prob de ocorrencia de 75%
            printf("[Processo %d]: Esperando seu tempo de rodar acabar.\n", processNumber);
        }

        info->PC++;
        usleep(500000);
    }

    // Fecha tudo e sai

    close(fifoan);

    shmdt(info);

    _exit(0);
}