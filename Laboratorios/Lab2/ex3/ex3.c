#include <stdio.h>
#include <sys/shm.h> // Para shmget(), shmat(), shmdt() e shmctl()
#include <stdlib.h> // Para exit e NULL
#include <sys/ipc.h> // Para flags IPC_CREAT, IPC_EXCL, IPC_NOWAIT e estrutura ipc_perm
#include <sys/stat.h> // Para flags de permissão
#include <unistd.h> // Para fork() e getpid()
#include <sys/types.h> // Para tipos como pid_t
#include <sys/wait.h> // Para waitpid();

void buscaVet(int *shm_ptrVet, int *shm_ptrPos, int chaveBuscada, int numFilho) {
    int posUniversal;

    for (int i = 0; i < 5 ; i++) {
        posUniversal = numFilho*5 + i;
        if (shm_ptrVet[posUniversal] == chaveBuscada) {
            *shm_ptrPos = posUniversal;
            break;
        }
    }

    shmdt(shm_ptrVet);
    shmdt(shm_ptrPos);

    _exit(0);
}

int main(void) {
    int vet[20] = {19, 1, 6, 8, 3, 7, 9, 2, 18, 15, 11, 14, 13, 4 , 5, 10, 17, 12, 16, 20};
    int chaveBuscada = 12;
    int size = sizeof(vet);

    int pos;
    int i;

    pid_t filhos[4];

    int shmidVet = shmget(IPC_PRIVATE, (size_t) size, (IPC_CREAT | IPC_EXCL | S_IRUSR | S_IWUSR)); // Read-write pelo dono, cria se não existe, falha se existe.
    if (shmidVet < 0) {
        perror("Erro ao criar segmento de memória compartilhada. Saindo...");
        exit(-1);
    }

    int *shm_ptrVet = shmat(shmidVet, NULL, 0);

    if (shm_ptrVet < 0) {
        perror("Erro ao anexar segmento de memória compartilhada. Saindo...");

        shmctl(shmidVet, IPC_RMID, NULL); // Remove o segmento de memória compartilhada em caso de erro

        exit(-2);
    }

    int shmidPos = shmget(IPC_PRIVATE, (size_t) sizeof(int), (IPC_CREAT | IPC_EXCL | S_IRUSR | S_IWUSR)); // Read-write pelo dono, cria se não existe, falha se existe.
    if (shmidPos < 0) {
        perror("Erro ao criar segmento de memória compartilhada. Saindo...");
        exit(-3);
    }

    int *shm_ptrPos = shmat(shmidPos, NULL, 0);

    if (shm_ptrPos < 0) {
        perror("Erro ao anexar segmento de memória compartilhada. Saindo...");

        shmctl(shmidPos, IPC_RMID, NULL); // Remove o segmento de memória compartilhada em caso de erro

        exit(-4);
    }

    *shm_ptrPos = -1;

    for (i = 0; i < sizeof(vet)/sizeof(int); i++) {
        shm_ptrVet[i] = vet[i];
    }

    for (i = 0; i < 4; i++) {
        filhos[i] = fork();

        if (filhos[i] < 0) {
            perror("Erro ao criar um filho. Saindo..."); 
            exit(-5); 
        }

        if (filhos[i] == 0) {
            // Area dos filhos
            buscaVet(shm_ptrVet, shm_ptrPos, chaveBuscada, i);
        }

    }

    // Depois de criar sequencialmente os 4 filhos, que já podem estar (ou já estão) rodando pois foram escalonados pelo kernel,
    // Pai espera cada um deles terminar para seguir com o fluxo de mostrar se a chave foi encontrada ou não, e aonde foi encontrada.
    // Isso garante que todos os filhos sejam criados e podem estar executando paralelamente
    // No waitpid abaixo, pai espera explicitamente filhos[i] terminar de rodar (ou ser interrompido)
    // Mas isso não significa que os outros filhos não estejam rodando paralelamente a ele.    
    for(i = 0; i < 4; i++) {
        waitpid(filhos[i], NULL, 0);
    }

    pos = *shm_ptrPos;

    if (pos != -1) {
        printf("Chave %d foi encontrada no vetor na posição %d.\n", chaveBuscada, pos);
    }

    else {
        printf("Chave %d não foi encontrada no vetor.\n", chaveBuscada);
    }

    shmdt(shm_ptrVet);
    shmdt(shm_ptrPos);

    shmctl(shmidVet, IPC_RMID, NULL);
    shmctl(shmidPos, IPC_RMID, NULL);
}