#include <stdio.h>
#include <sys/types.h>
#include <sys/stat.h>
#include <fcntl.h>
#include <unistd.h>
#include <string.h>
#include <stdlib.h>
#include <signal.h>

#define BUFFER_SIZE 256
#define FIFO_ENTRADA "FIFO1"

static int fifo_in = -1;  // visível no handler

void handle_sigint(int sig) {
    // cleanup do servidor
    if (fifo_in >= 0) close(fifo_in);
    unlink(FIFO_ENTRADA);
    _exit(0); // usa _exit dentro de handler
}

int main(void) {
    char buffer[BUFFER_SIZE];

    // instala handler de Ctrl+C
    signal(SIGINT, handle_sigint);

    if (mkfifo(FIFO_ENTRADA, (S_IRUSR | S_IWUSR)) == 0) {
        puts("FIFO1 criada com sucesso.");
    }

    fifo_in = open(FIFO_ENTRADA, O_RDONLY); // bloqueante
    if (fifo_in < 0) {
        puts("Erro ao abrir FIFO1 para leitura.");
        return -1;
    }

    puts("Servidor rodando...");

    while (1) {
        int n = read(fifo_in, buffer, BUFFER_SIZE - 1);
        if (n > 0) {
            buffer[n] = '\0';

            // espera formato "pid:mensagem"
            char *sep = strchr(buffer, ':');
            if (!sep) continue;

            *sep = '\0';
            pid_t pid = atoi(buffer);
            char *msg = sep + 1;

            // para maiúsculo (sem ctype.h)
            for (int i = 0; msg[i]; i++) {
                if (msg[i] >= 'a' && msg[i] <= 'z') {
                    msg[i] = msg[i] - 'a' + 'A';
                }
            }

            // FIFO de resposta do cliente
            char fifo_resp[64];
            sprintf(fifo_resp, "FIFO_RESP_%d", pid);

            int fifo_out = open(fifo_resp, O_WRONLY); // bloqueante
            if (fifo_out < 0) {
                puts("Erro ao abrir FIFO de resposta.");
                continue;
            }

            write(fifo_out, msg, strlen(msg) + 1); // inclui '\0'
            close(fifo_out);

            printf("Servidor respondeu para %d: %s\n", pid, msg);
        }
    }

    // (normalmente não chega aqui; Ctrl+C chama o handler)
    if (fifo_in >= 0) close(fifo_in);
    unlink(FIFO_ENTRADA);
    return 0;
}
