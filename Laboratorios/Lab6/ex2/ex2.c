#include <stdio.h>
#include <sys/types.h> // Para tipos como pid_t
#include <sys/stat.h> // Para mkfifo(), S_IRUSR, S_IWUSR
#include <fcntl.h> // Para open(), O_RDONLY, O_WRONLY, O_NONBLOCK
#include <unistd.h> // Para fork() e getpid()
#include <sys/wait.h> //Para waitpid()
#include <string.h> // Para strlen()
#include <stdlib.h> // Para NULL

#define BUFFER_SIZE 256

int main(void) {
    int fifo;
    pid_t filho1, filho2 = 0;

    if (mkfifo("FIFO1", (S_IRUSR | S_IWUSR)) == 0) {
        puts ("FIFO criada com sucesso.");
    }

    filho1 = fork();

    if (filho1 != 0) {
        filho2 = fork();
    }

    printf("PID do processo rodando: %d\n", getpid());

    if (filho1 < 0 || filho2 < 0) {
        puts("Erro ao criar processos filhos.");
        return -1;
    }

    if ((filho1 != 0) && (filho2 != 0)) {
        //Processo pai
        
        int stringLidas = 0;
        int qualFilho;

        char buffer[BUFFER_SIZE];

        if ((fifo = open("FIFO1", (O_RDONLY | O_NONBLOCK))) < 0) {
            puts ("Erro ao abrir a FIFO para escrita.");
            return -1;
        }

        while (stringLidas < 2) {

            qualFilho = waitpid(-1, NULL, 0); // Espera qualquer filho terminar
            printf("Filho com PID %d terminou.\n", qualFilho);
            stringLidas++;

            while (read(fifo, buffer, BUFFER_SIZE) > 0) {
                printf("Mensagem lida pelo pai: %s\n", buffer);
            }
        }

        printf("Todos os filhos terminaram. Processo pai encerrando.\n");
        return 0;
    }

    else {
        //Processos filhos

        char mensagem[BUFFER_SIZE];
        size_t size;

        printf("Digite a mensagem para o processo pai:\n");

        scanf("%255s", mensagem); // Lê uma string (máximo 255 caracteres)
        size = strlen(mensagem);

        mensagem[size] = '\n'; // Garante newline
        mensagem[size + 1] = '\0'; // Garante null terminator

        if ((fifo = open("FIFO1", O_WRONLY)) < 0) {
            puts ("Erro ao abrir a FIFO para escrita.");
            return -1;
        }

        write(fifo, mensagem, size + 2); // +2 para incluir newline e null terminator

        printf("Filho com PID %d enviou a mensagem: %s\n", getpid(), mensagem);
        return 0;
    }
}