#include <stdio.h>
#include <sys/shm.h> // Para shmget(), shmat(), shmdt() e shmctl()
#include <stdlib.h> // Para exit e NULL
#include <sys/ipc.h> // Para flags IPC_CREAT, IPC_EXCL, IPC_NOWAIT e estrutura ipc_perm

int main(void) {
    int shmid = shmget((key_t)8752, 0, 0);  // só anexa, não cria)

    char *shm_ptr = (char *) shmat(shmid, NULL, 0);

    if (shm_ptr < 0) {
        perror("Erro ao anexar segmento de memória compartilhada. Saindo...");
        exit(-6);
    }

    printf("Printando mensagem do dia: %s\n", shm_ptr);
    
    shmdt(shm_ptr);
    
    return 0;
}