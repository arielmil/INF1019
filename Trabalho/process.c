#include <stdio.h>
#include <stdlib.h>     // Para exit e _exit
#include <unistd.h>     // Para sleep, write, close
#include <fcntl.h>      // Para open
#include <string.h>     // Para strlen
#include <time.h>       // Para srand, rand
#include <sys/types.h>  // Para pid_t

// Convenção de nomes de FIFOs
#define FIFOAN1 "FIFOAN1"  // Canal para dispositivo: '1' ou '2'
#define FIFOAN2 "FIFOAN2"  // Canal para operação: 'R','W','X'

// Parâmetros do processo de aplicação (exemplo)
#define MAX_PC 25          // Cada processo executa 25 iterações

// Nota: este processo não decide parar sozinho por syscall.
// Ele apenas envia um "pedido" (D,Op). O KernelSim é quem para/retoma via sinais.

int main(int argc, char *argv[]) {
    if (argc < 2) {
        fprintf(stderr, "[process]: uso: %s <processNumber>\n", argv[0]);
        _exit(-1);
    }

    int processNumber = atoi(argv[1]);

    int fifoan1 = open(FIFOAN1, O_WRONLY);
    if (fifoan1 < 0) {
        perror("[process]: erro ao abrir FIFOAN1");
        _exit(-2);
    }

    int fifoan2 = open(FIFOAN2, O_WRONLY);
    if (fifoan2 < 0) {
        perror("[process]: erro ao abrir FIFOAN2");
        _exit(-3);
    }

    srand((unsigned int) (getpid() ^ time(NULL)));

    int pc = 0;

    while (pc < MAX_PC) {
        // Simula trabalho de 1 segundo por iteração
        usleep(500000);

        // Baixa probabilidade de solicitar E/S (≈ 15%)
        int p = (rand() % 100) + 1;
        if (p < 15) {
            char d;
            int choose = (rand() % 100) + 1;
            if (choose < 67) {
                d = '1'; // D1 mais provável
            } 
            
            else {
                d = '2';
            }

            char op;
            int r = rand() % 3;
            if (r == 0) {
                op = 'R';
            } 
            
            else if (r == 1) {
                op = 'W';
            } 
            
            else {
                op = 'X';
            }

            int w1 = write(fifoan1, &d, 1);
            
            if (w1 != 1) {
                perror("[process]: erro ao escrever em FIFOAN1");
            }

            int w2 = write(fifoan2, &op, 1);
            if (w2 != 1) {
                perror("[process]: erro ao escrever em FIFOAN2");
            }
        }

        pc = pc + 1;
        
        usleep(500000);
    }

    close(fifoan1);
    close(fifoan2);

    _exit(0);
}
