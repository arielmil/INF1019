#include <sys/sem.h> //Para semget(), semctl() e semop()
#include <sys/shm.h> // Para shmget(), shmat(), shmdt() e shmctl()
#include <unistd.h> // Para fork() e getpid()
#include <sys/types.h> // Para tipos como pid_t
#include <sys/wait.h> // Para waitpid()
#include <signal.h> // Para signal()
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

// Sai do programa por interrupção de usuário
void intHandler(int signal) {
    delSem(semid);
    _exit(0);
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

    char *shm_ptr;

    union semun semUnion;

    shmid = shmget(IPC_PRIVATE, 17 * sizeof(char), IPC_CREAT | IPC_EXCL | S_IRUSR | S_IWUSR); // Read-write pelo dono, cria se não existe, falha se existe.
    if (shmid < 0) {
        perror("Erro ao tentar usar shmget. Saindo...");
        exit(-1);
    }

    shm_ptr = (char *)shmat(shmid, NULL, 0);
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

    // O valor deste semaforo é definido assim para poder entrar pela primeira vez na RC do produtor
    // A partir da entrada na RC do produtor, é liberada a RC do consumidor
    // O consumidor então quando entrar na sua RC, libera a RC do produtor

    // Define o valor inicial do semáforo consumidor como 0
    semUnion.val = 0;
    test = semctl(semid, 1, SETVAL, semUnion);
    if (test < 0) {
        perror("Erro ao tentar usar semctl. Saindo...");
        exit(-4);
    }

    if (signal(SIGINT, intHandler) == SIG_ERR) {
        perror("Erro ao usar signal. Saindo...");
        shmctl(shmid, IPC_RMID, NULL);
        delSem(semid);
        exit(-5);
    }

    filho = fork();
    if (filho < 0) {
        perror("Erro ao usar fork. Saindo...");
        shmctl(shmid, IPC_RMID, NULL);
        delSem(semid);
        exit(-6);
    }

    // Região do pai (Processo escritor)
    if (filho == 0) {
        char c;
        while(1) {
            // Espera para poder entrar na região crítica protegida pelo semaforo 0 (Responsável pela RC do Produtor)
            down(semid, 0);

            for (int i = 0; i < 16; i++) {
                scanf("%c", &c);
                shm_ptr[i] = c;
            }

            // Para terminação de string
            shm_ptr[16] = '\0';

            // Libera a região crítica protegida pelo semaforo 1 (Responsável pela RC do Consumidor)
            up(semid, 1);
        }
    }

    // Região do filho (Processo leitor)  
    else {
        while(1) {
            // Espera para poder entrar na região crítica protegida pelo semaforo 1 (Responsável pela RC do Consumidor)
            down(semid, 1);

            printf("Printando shm_ptr:\n");
            printf("%s\n", shm_ptr);

            // Libera a região crítica protegida pelo semaforo 0 (Responsável pela RC do Produtor)
            up(semid, 0);
        }
    }

    /* 
    
        OBS: Usar dois semaforos garante que apenas o consumidor pode liberar a RC do produtor,
             e apenas o produtor pode liberar a RC do consumidor.
    
    */
}