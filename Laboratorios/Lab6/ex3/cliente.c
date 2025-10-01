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

static int fifo_in  = -1;      // visíveis no handler
static int fifo_out = -1;
static char fifo_resp[64];

void handle_sigint(int sig) {
    // cleanup do cliente
    if (fifo_in  >= 0) close(fifo_in);
    if (fifo_out >= 0) close(fifo_out);
    if (fifo_resp[0])  unlink(fifo_resp);
    _exit(0); // usa _exit dentro de handler
}

int main(void) {
    char buffer[BUFFER_SIZE];
    pid_t pid = getpid();

    // instala handler de Ctrl+C
    signal(SIGINT, handle_sigint);

    // cria FIFO exclusiva de resposta
    sprintf(fifo_resp, "FIFO_RESP_%d", pid);
    if (mkfifo(fifo_resp, (S_IRUSR | S_IWUSR)) == 0) {
        printf("FIFO de resposta %s criada.\n", fifo_resp);
    }

    // conecta ao servidor (escrita) - não bloqueante
    fifo_in = open(FIFO_ENTRADA, O_WRONLY | O_NONBLOCK);
    if (fifo_in < 0) {
        puts("Servidor não está rodando.");
        unlink(fifo_resp);
        return -1;
    }

    // abre a própria FIFO de resposta (leitura) - não bloqueante
    fifo_out = open(fifo_resp, O_RDONLY | O_NONBLOCK);
    if (fifo_out < 0) {
        puts("Erro ao abrir FIFO de resposta.");
        unlink(fifo_resp);
        return -1;
    }

    while (1) {
        printf("Digite uma palavra: ");
        if (!fgets(buffer, BUFFER_SIZE, stdin)) break;

        buffer[strcspn(buffer, "\n")] = '\0';

        // mensagem "pid:texto"
        char mensagem[BUFFER_SIZE];
        sprintf(mensagem, "%d:%s", pid, buffer);

        write(fifo_in, mensagem, strlen(mensagem) + 1); // inclui '\0'

        int n;
        while ((n = read(fifo_out, buffer, BUFFER_SIZE - 1)) <= 0) {
            usleep(10000); // espera resposta
        }
        buffer[n] = '\0';
        printf("Resposta: %s\n", buffer);
    }

    // cleanup normal (se sair sem Ctrl+C)
    if (fifo_in  >= 0) close(fifo_in);
    if (fifo_out >= 0) close(fifo_out);
    if (fifo_resp[0])  unlink(fifo_resp);
    return 0;
}
