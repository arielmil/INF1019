#include <stdio.h>
#include <sys/types.h>
#include <sys/stat.h>
#include <fcntl.h>

int main(void) {

    int fifo;
    char ch;

    if ((fifo = open("FIFO1", (O_WRONLY | O_NONBLOCK))) < 0) {
        puts ("Erro ao abrir a FIFO para escrita.");
        return -1;
    }

    while(1) {
        scanf("%c", &ch);
        write(fifo, &ch, sizeof(ch));
    }

    return 0;
}