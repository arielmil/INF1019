#include <sys/sem.h> //Para semget(), semctl() e semop()
#include <sys/shm.h> // Para shmget(), shmat(), shmdt() e shmctl()
#include <unistd.h> // Para fork() e getpid()
#include <sys/types.h> // Para tipos como pid_t
#include <sys/wait.h> // Para waitpid()
#include <sys/stat.h> // Para flags de controle
#include <stdio.h>
#include <stdlib.h>

int semid;
int shmid;

union semun { 
    int val; 
    struct semid_ds *buf; 
    ushort *array; 
};

// Deleta os semaforos
void delSem(int semid) { 
    union semun semUnion; 
    semctl(semid, 0, IPC_RMID, semUnion);
    semctl(semid, 1, IPC_RMID, semUnion);
}

// Incrementa um semaforo, e desfaz a operação caso de problema
int up(int semid, int semnum) {
    struct sembuf semB; 
    semB.sem_num = semnum; 
    semB.sem_op = 1; 
    semB.sem_flg = SEM_UNDO; 
    semop(semid, &semB, 1);
    return 0;
}

// Decrementa um semaforo, e desfaz a operação caso de problema
int down(int semid, int semnum) {
    struct sembuf semB; 
    semB.sem_num = semnum; 
    semB.sem_op = -1; 
    semB.sem_flg = SEM_UNDO; 
    semop(semid, &semB, 1);
    return 0;
}

int main(void) {
    int test;

    pid_t filho;

    int *shm_ptr;

    union semun semUnion;

    shmid = shmget(IPC_PRIVATE, sizeof(int), IPC_CREAT | IPC_EXCL | S_IRUSR | S_IWUSR); // Read-write pelo dono, cria se não existe, falha se existe.
    if (shmid < 0) {
        perror("Erro ao tentar usar shmget. Saindo...");
        exit(-1);
    }

    shm_ptr = (int *)shmat(shmid, NULL, 0);
    if (shm_ptr == (void *) -1) {
        perror("Erro ao tentar usar shmat. Saindo...");
        shmctl(shmid, IPC_RMID, NULL);
        exit(-2);
    }

    // Cria o semaforo, ou obtém se já existe
    semid = semget(IPC_PRIVATE, 2, (IPC_CREAT | IPC_EXCL | 0666));
    if (semid < 0) {
        perror("Erro ao tentar usar semget. Saindo...");
        exit(-3);
    }

    // Define o valor inicial do semáforo produtor como 1
    semUnion.val = 1;
    test = semctl(semid, 0, SETVAL, semUnion);
    if (test < 0) {
        perror("Erro ao tentar usar semctl. Saindo...");
        exit(-4);
    }

    // Define o valor inicial do semáforo soma 5 como 0
    semUnion.val = 0;
    test = semctl(semid, 1, SETVAL, semUnion);
    if (test < 0) {
        perror("Erro ao tentar usar semctl. Saindo...");
        exit(-4);
    }

    *shm_ptr = 0;
    printf("Valor inicial da shmem: %d\n", *shm_ptr);

    filho = fork();
    if (filho < 0) {
        perror("Erro ao usar fork. Saindo...");
        shmctl(shmid, IPC_RMID, NULL);
        delSem(semid);
        exit(-6);
    }

    // Região do pai (Processo soma 5)
    if (filho == 0) {
        down(semid, 0);

        printf("Valor da shmem: %d\n", *shm_ptr);
        printf("Processo soma 5 atuando...\n");

        *shm_ptr = *shm_ptr + 5;

        up(semid, 1);
    }

    // Região do filho (Processo soma 1)  
    else {
        down(semid, 1);

        printf("Valor da shmem: %d\n", *shm_ptr);
        printf("Processo soma 1 atuando...\n");

        (*shm_ptr)++;

        up(semid, 0);
    }

    printf("Valor da shmem final: %d\n", *shm_ptr);

    delSem(semid);
    shmdt(shm_ptr);
    shmctl(shmid, IPC_RMID, NULL);
}