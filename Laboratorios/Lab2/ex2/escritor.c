#include <stdio.h>
#include <sys/shm.h> // Para shmget(), shmat(), shmdt() e shmctl()
#include <stdlib.h> // Para exit e NULL
#include <sys/ipc.h> // Para flags IPC_CREAT, IPC_EXCL, IPC_NOWAIT e estrutura ipc_perm
#include <sys/stat.h> // Para flags de permissão
#include <unistd.h> // Para fork() e getpid()
#include <sys/types.h> // Para tipos como pid_t
#include <sys/wait.h> // Para waitpid();
#include <string.h> // Para strcpy

int main(void) {
    FILE *fptr;
    char buffer[256];

    fptr = fopen("mensagem.txt", "r");
    if (fptr == NULL) {
        perror("Erro ao abrir o arquivo. Saindo...");
        exit(-1);
    }

    if (fgets(buffer, sizeof(buffer), fptr) == NULL) {
        perror("Erro ao ler do arquivo. Saindo...");
        exit(-2);
    }

    fclose(fptr);

    int shmid = shmget((key_t) 8752, (size_t) strlen(buffer) + 1, (IPC_CREAT | IPC_EXCL | S_IRUSR | S_IWUSR)); // Read-write pelo dono, cria se não existe, falha se existe.

    if (shmid < 0) {
        perror("Erro ao criar segmento de memória compartilhada. Saindo...");
        exit(-3);
    }

    char *shm_ptr = (char *) shmat(shmid, NULL, 0);

    if (shm_ptr < 0) {
        perror("Erro ao anexar segmento de memória compartilhada. Saindo...");

        shmctl(shmid, IPC_RMID, NULL); // Remove o segmento de memória compartilhada em caso de erro

        exit(-4);
    }

    strcpy(shm_ptr, buffer);

    pid_t filho = fork();
    if (filho < 0) {
        perror("Erro ao criar filho. Saindo...");
        exit(-5);
    }

    if (filho != 0) {
        // Região do pai
        filho = waitpid(filho, NULL, 0);

        shmdt(shm_ptr); // Desanexa o segmento de memória compartilhada
        shmctl(shmid, IPC_RMID, NULL); // Remove o segmento de memória compartilhada

        return 0;
    }

    // Região do filho
    char *argv[] = { "cliente", NULL };
    execv("./cliente", argv);
    
    perror("Erro ao executar cliente. Saindo...");
    _exit(-7);

}