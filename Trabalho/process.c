#include <stdio.h>
#include <stdlib.h> // Para exit e NULL
#include <unistd.h> // Para getpid() e usleep()
#include <time.h> // Para time()
#include <fcntl.h> // Para flags de controle de FIFO.
#include "info.h"

/* OBS: Usarei duas FIFOS para IPC entre process e KS:

        FIFOAN1 é usada para indicar qual dispositivo process está tentando acessar:

            1: Dispositivo D1
            2: Dispositivo D2

        FIFOAN2 é usada para indicar qual operação process deseja fazer no dispositivo selecionado:

            'R': Leitura
            'W': Escrita
            'X': Execução

        KS deve atualizar o campo PC sempre que receber mensagens por uma das duas FIFOS,
        porém deve apenas atualizar os campos timesD1acessed e timesD2acessed se uma mensagem por FIFO2 for enviada.
*/

// OBS: argv[0]: ./process, argv[1]: processNumber
int main(int argc, char *argv[]) {
    char D, Op;

    int PC = 0;
    int d;
    int fifoan1, fifoan2;
    int processNumber; // [1..5]

    pid_t pid = getpid();
    
    if (argc < 2) {
        perror("Erro: Processo não recebeu o número de argumentos necessários. Saindo...\n");
        _exit(-1);
    }

    processNumber = atoi(argv[1]);

    fifoan1 = open("FIFOAN1", O_WRONLY); // Abre uma FIFO em modo de escrita bloqueante para sinalizar ao kernel a espera de IRQ0
    if (fifoan1 < 0) {
        perror("Erro ao abrir a FIFO 'FIFOAN1' para escrita. Saindo...\n");
        _exit(-2);
    }

    fifoan2 = open("FIFOAN2", O_WRONLY); // Abre uma FIFO em modo de escrita bloqueante para sinalizar ao kernel a espera de IRQ0
    if (fifoan2 < 0) {
        perror("Erro ao abrir a FIFO 'FIFOAN2' para escrita. Saindo...\n");
        close(fifoan1);
        _exit(-3);
    }

    srand((unsigned)(pid ^ time(NULL))); // Para seedar a função rand() 

    while (PC < MAX) {
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
            write(fifoan1, &D, 1);
            write(fifoan2, &Op, 1);

        }

        else {
            // Prob de ocorrencia de 75%
            printf("[Processo %d]: Esperando seu tempo de rodar acabar.\n", processNumber);
        }

        PC++;
        usleep(500000);
    }

    close(fifoan1);
    close(fifoan2);

    _exit(0);
}