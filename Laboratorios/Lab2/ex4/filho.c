#include <stdio.h>
#include <sys/shm.h> // Para shmget(), shmat(), shmdt() e shmctl()
#include <stdlib.h> // Para exit NULL e rand()
#include <sys/ipc.h> // Para flags IPC_CREAT, IPC_EXCL, IPC_NOWAIT e estrutura ipc_perm
#include <sys/stat.h> // Para flags de permissão
#include <unistd.h> // Para sleep
#include <string.h> // Para strcmp
#include "struct.h" // Para pai e filho poderem referenciar a struct

int main(int argc, char *argv[]) {
    int shmidM;
    M *m;

    if (argc < 2) { 
        fprintf(stderr, "uso: filho 1|2\n");
        exit(1); 
    }

    if (strcmp(argv[1], "1") == 0) {
        // Filho1
        shmidM = shmget(11, (size_t) sizeof(M), (S_IRUSR | S_IWUSR)); // Read-write pelo dono, cria se não existe, falha se existe.

    }

    else {
        // Filho2
        shmidM = shmget(22, (size_t) sizeof(M), (S_IRUSR | S_IWUSR)); // Read-write pelo dono, cria se não existe, falha se existe.
    }

    if (shmidM < 0) {
        perror("Erro em shmget. Saindo...");
        exit(-4);
    }

    m = (M *) shmat(shmidM, NULL, 0);
    if (m == (void *) -1) {
        perror("Erro ao usar shmat. Saindo...");
        exit(-5);
    }

    int value;
    while(m->seq < 20) {
        value = rand()%3;
        sleep(value);

        value = value + rand()%30;

        m->value = value;
        m->seq++;
    }

    shmdt(m);

    _exit(0);
}