#include <stdio.h>
#include <sys/shm.h> // Para shmget(), shmat(), shmdt() e shmctl()
#include <stdlib.h> // Para exit e NULL
#include <sys/ipc.h> // Para flags IPC_CREAT, IPC_EXCL, IPC_NOWAIT e estrutura ipc_perm
#include <sys/stat.h> // Para flags de permissão
#include <unistd.h> // Para fork() e getpid()
#include <sys/types.h> // Para tipos como pid_t
#include <sys/wait.h> //Para waitpid()
#include "struct.h" // Para pai e filho poderem referenciar a struct

int main (void) {

    int shmidM1 = shmget(11, (size_t) sizeof(M), (IPC_CREAT | IPC_EXCL | S_IRUSR | S_IWUSR)); // Read-write pelo dono, cria se não existe, falha se existe.
    int shmidM2 = shmget(22, (size_t) sizeof(M), (IPC_CREAT | IPC_EXCL | S_IRUSR | S_IWUSR)); // Read-write pelo dono, cria se não existe, falha se existe.

    if (shmidM1 < 0 || shmidM2 < 0) {
        perror("Erro em shmget. Saindo...");

        // Limpa o programa para a próxima execução
        shmctl(shmidM1, IPC_RMID, NULL);
        shmctl(shmidM2, IPC_RMID, NULL);

        exit(-1);
    }

    M *m1 = (M *) shmat(shmidM1, NULL, 0);
    M *m2 = (M *) shmat(shmidM2, NULL, 0);

    if (m1 == (void *) -1 || m2 == (void *) -1) {
        perror("Erro ao anexar segmento de memória compartilhada.");
        // Remove o segmento de memória compartilhada em caso de erro
        shmctl(shmidM1, IPC_RMID, NULL);
        shmctl(shmidM2, IPC_RMID, NULL);

        exit(-2);
    }

    m1->seq = 0;
    m2->seq = 0;

    pid_t filho1;
    pid_t filho2;

    filho1 = fork();
    if (filho1 < 0) {
        puts("Erro ao criar processo filho1.");
        exit (-3);
    }

    if (filho1 != 0) {
        // Processo pai
        filho2 = fork();
        if (filho2 < 0) {
            puts("Erro ao criar processo filho2.");
            exit (-3);
        }
    }

    else {
        // Area do filho1
        char *argv[] = { "filho", "1", NULL};
        execv("./filho", argv);

        //Em caso de erro:
        perror("Erro ao usar execv na area do filho1");
        _exit(-6);
    }

    if (filho2 == 0) {
        // Area do filho2
        char *argv[] = { "filho", "2", NULL};
        execv("./filho", argv);

        //Em caso de erro:
        perror("Erro ao usar execv na area do filho1");
        _exit(-7);
    }

    int seq1 = m1->seq;
    int seq2 = m2->seq;

    while(seq1 < 20 && seq2 < 20) {
        if (seq1 < m1->seq && seq2 < m2->seq) {
            printf("Valor do produto: m1->value * m2->value = %d\n", m1->value * m2->value);

            seq1++;
            seq2++;
        }

        usleep(1e4);
    }

    shmdt(m1);
    shmdt(m2);
    
    // Para garantir que ambos os filhos acabem de rodar antes de usar shmctl  
    waitpid(filho1, NULL, 0);
    waitpid(filho2, NULL, 0);

    shmctl(shmidM1, IPC_RMID, NULL);
    shmctl(shmidM2, IPC_RMID, NULL);

    return 0;
}