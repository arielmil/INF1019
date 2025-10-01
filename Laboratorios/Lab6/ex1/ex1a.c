#include <stdio.h>
#include <sys/types.h>
#include <sys/stat.h>
#include <fcntl.h>

//OBS: ex1a cria a FIFO, ex1b abre a FIFO criada por ex1a.

int main(void) {

    int fifo;
    char ch;

    if (mkfifo("FIFO1", (S_IRUSR | S_IWUSR)) == 0) {
        puts ("FIFO criada com sucesso.");
    } 

    if ((fifo = open("FIFO1", O_RDONLY)) < 0) {
        puts ("Erro ao abrir a FIFO para leitura.");
        return -1;
    }

    while(1) {
        read(fifo, &ch, 1);
        printf("%c", ch);
    }

    return 0;
}