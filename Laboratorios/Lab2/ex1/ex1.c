#include <stdio.h>
#include <sys/shm.h> // Para shmget(), shmat(), shmdt() e shmctl()
#include <stdlib.h> // Para exit e NULL
#include <sys/ipc.h> // Para flags IPC_CREAT, IPC_EXCL, IPC_NOWAIT e estrutura ipc_perm
#include <sys/stat.h> // Para flags de permissão
#include <unistd.h> // Para fork() e getpid()
#include <sys/types.h> // Para tipos como pid_t
#include <sys/wait.h> //Para waitpid()

#define VET_SIZE 3
#define NUM_MATRIZES 3

void trabalhoInfantil(int numFilho, int *shm_ptr) {
    printf("PID do rapaz mirim: %ld (linha %d)\n", (long)getpid(), numFilho);

    for (int i = 0; i < 3; i++) {
        shm_ptr[numFilho*3 + 18 + i] = shm_ptr[numFilho*3 + i] + shm_ptr[numFilho*3 + 9 + i];
    }

    shmdt(shm_ptr);

    _exit(0); // Termina apenas o processo filho!
}

void printaVet(int *vet, int size) {
    printf("{ ");

    int i;

    for (i = 0; i < size - 1; i++) {
        printf("%d, ", vet[i]);
    }

    printf("%d }", vet[i]);
}

void printaMatriz(int *shm_ptr, int indiceMatriz) {

    for (int i = 0; i < 3; i++) {
        printf("[");
        printaVet(&shm_ptr[(indiceMatriz * 9) + i*3], 3);
        printf("]");
        printf("\n");
    }

    printf("\n");
}

int main(void) {

    int v11[VET_SIZE], v12[VET_SIZE], v13[VET_SIZE];
    int v21[VET_SIZE], v22[VET_SIZE], v23[VET_SIZE];
    int vsol1[VET_SIZE], vsol2[VET_SIZE], vsol3[VET_SIZE];

    int i;

    //Tamanho total: 27 inteiros

    // Inicializa os vetores
    for (i = 0; i < VET_SIZE; i++) {
        v11[i] = i + 1;
        v12[i] = (i + 1) * 2;
        v13[i] = (i + 2) * 3;

        v21[i] = (i + 3) * 4;
        v22[i] = (i + 4) * 5;
        v23[i] = (i + 5) * 6;

        vsol1[i] = 0;
        vsol2[i] = 0;
        vsol3[i] = 0;
    }

    // Cria segmento de memória compartilhada
    int shmid = shmget(IPC_PRIVATE, 27 * sizeof(int), IPC_CREAT | IPC_EXCL | S_IRUSR | S_IWUSR); // Read-write pelo dono, cria se não existe, falha se existe.
    if (shmid < 0) {
        perror("Erro ao criar segmento de memória compartilhada.");
        exit(1);
    }

    // Anexa o segmento de memória compartilhada
    int *shm_ptr = (int *) shmat(shmid, NULL, 0);
    if (shm_ptr == (int *) -1) {
        perror("Erro ao anexar segmento de memória compartilhada.");
        // Remove o segmento de memória compartilhada em caso de erro
        shmctl(shmid, IPC_RMID, NULL);
        exit(1);
    }

    // Copia os vetores para a memória compartilhada
    for (i = 0; i < VET_SIZE; i++) {
        shm_ptr[i] = v11[i];
        shm_ptr[i + VET_SIZE] = v12[i];
        shm_ptr[i + (2 * VET_SIZE)] = v13[i];

        shm_ptr[i + (3 * VET_SIZE)] = v21[i];
        shm_ptr[i + (4 * VET_SIZE)] = v22[i];
        shm_ptr[i + (5 * VET_SIZE)] = v23[i];

        shm_ptr[i + (6 * VET_SIZE)] = vsol1[i];
        shm_ptr[i + (7 * VET_SIZE)] = vsol2[i];
        shm_ptr[i + (8 * VET_SIZE)] = vsol3[i];
    }

    printf("\nPrintando a Matriz antes dos filhos mexerem:\n");
    printaMatriz(shm_ptr, 0);
    printaMatriz(shm_ptr, 1);
    printaMatriz(shm_ptr, 2);

    pid_t filhos[3];
    
    for (i = 0; i < 3; i++) {
        filhos[i] = fork();

        if (filhos[i] < 0) {
            perror("Erro ao fazer o fork. Saindo do programa...");
            exit(-1);
        }

        if (filhos[i] == 0) {
            /* Região dos filhos */
            trabalhoInfantil(i, shm_ptr);
        }

    }

    for (i = 0; i < 3; i++) {
        waitpid(filhos[i], NULL, 0); // Espera especificamente o filho filhos[i] terminar o seu trabalhoInfantil()
    }

    printf("\nPrintando a Matriz depois dos filhos mexerem:\n");
    printaMatriz(shm_ptr, 0);
    printaMatriz(shm_ptr, 1);
    printaMatriz(shm_ptr, 2);

    shmdt(shm_ptr);

    shmctl(shmid, IPC_RMID, NULL);
}

/* 

OBS: Poderia fazer também com várias variaveis apontando para vários espaçoes de shm (Com 9 vetores)
Mas achei assim uma forma mais simplificada de fazer.

*/