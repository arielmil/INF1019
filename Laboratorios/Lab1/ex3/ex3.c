#include <stdio.h>
#include <unistd.h> // Para fork() e getpid()
#include <sys/types.h> // Para tipos como pid_t
#include <sys/wait.h> //Para waitpid()
#include <stdlib.h> // Para NULL

void printaVetor(char *vet, int n) {
    for (int i = 0; i < n; i++) {
        printf("vet[%d] = %d\n", i, vet[i]);
    }
}

void ordena(char *vet, int n) {
    for (int i = 0; i < n - 1; i++) {
        for (int j = 0; j < n - i - 1; j++) {
            if (vet[j] > vet[j + 1]) {
                char temp = vet[j];
                vet[j] = vet[j + 1];
                vet[j + 1] = temp;
            }
        }
    }
}

int main(void) {

    char vet[10] = {9, 7, 3, 5, 6, 2, 1, 4, 8, 0};

    printf("Vetor inicial:\n");
    printaVetor(vet, 10);

    pid_t filho;

    filho = fork();
    if (filho < 0) {
        puts("Erro ao criar processo filho.");
        exit(-1);
    }

    if (filho != 0) {
        // Processo pai
        printf("PID do processo pai: %d\n", getpid());
        waitpid(-1, NULL, 0); // Espera o filho terminar

        printf("Vetor no processo pai depois do filho ordenar:\n");
        printaVetor(vet, 10);

        printf("Processo pai encerrando.\n");
        exit(0);
    }

    // Processo filho
    printf("PID do processo filho: %d\n", getpid());

    ordena(vet, 10);

    printf("Vetor no processo filho depois de ordenar:\n");
    printaVetor(vet, 10);
    
    printf("Processo filho encerrando.\n");
    
    exit(0);
}

/* 

    Os valores devem ser diferentes pois cada processo (pai e filho) tem uma pilha de váriaveis locais independente um do outro. Aparentemente 
    Isso também ocorre em váriaveis que guardam um endereço de memória. Acredito que este endereço provavelmente é um endereço virtual, e que o kernel deve ser responsável
    por traduzir este endereço virtual para o real, e o endereço real desta váriavel para o pai e para o filho sejam distintos.
    (Perguntar para o professor) 

*/