#include <stdio.h>
#include <unistd.h> // Para fork() e pipe()
#include <stdlib.h>
#include <string.h> // Para strlen
#include <sys/types.h> // Para tipos como pid_t

int main(void) {
    int fd[2];
    int test = pipe(fd);
    int sizeMensagem;

    if (test < 0) {
        perror("Error em pipe. Saindo...");
        exit(-1);
    }

    pid_t filho = fork();
    if (filho < 0) {
        perror("Erro ao criar filho. Saindo...");
    }

    if (filho == 0) {
        // Area do filho

        // Fecha a ponta de leitura na area do filho para evitar espera ocupada
        close(fd[0]);

        char mensagem[] = "Estou passando uma mensagem do processo filho para o processo pai via pipe!";
        sizeMensagem = sizeof(mensagem);

        test = write(fd[1], mensagem, sizeMensagem);
        close(fd[1]);

        if (test != sizeMensagem) {
            perror("Erro ao escrever a mensagem. Saindo...");
            exit(-2);
        }

        _exit(0);
    }

    // Area do pai

    // Fecha a ponta de escrita na area do pai para evitar espera ocupada
    close(fd[1]);

    char buffer[256];
    char *mensagem;

    test = read(fd[0], buffer, sizeof(buffer));
    close(fd[0]);

    if (test < 0) {
        perror("Erro ao ler a mensagem. Saindo...");
        exit(-3);
    }

    sizeMensagem = strlen(buffer) + 1; // +1 para incluir o '\0'.
    mensagem = (char *) malloc(sizeof(char) * sizeMensagem);

    if (mensagem == NULL) {
        perror("Erro ao usar malloc. Saindo...");
        exit(-4);
    }

    int i;
    for(i = 0; buffer[i] != '\0'; i++) {
        mensagem = buffer[i];
    }

    mensagem[i] = '\0';

    printf("Printando mensagem passada do filho para o pai:\n\n%s\n", mensagem);

    free(mensagem);

    return 0;
}