#include <stdio.h>
#include <stdlib.h> // Para exit e _exit
#include <signal.h> // Para kill() e signal()

void zeroHandler(int signo) {
    perror("Erro matematico. Mamae disse que nao pode dividir por zero. Saindo...\n");
    exit(0);
}

int main(int argc, char *argv[]) {
    signal(SIGFPE, zeroHandler);

    int var1, var2;

    var1 = atoi(argv[1]);
    var2 = atoi(argv[2]);

    printf("Quatro operacoes basicas:\n");

    printf("Soma: \n");
    printf("%d + %d = %d\n", var1, var2, var1 + var2);

    printf("Subtracao:\n");
    printf("%d - %d = %d\n", var1, var2, var1 - var2);

    printf("Multiplicacao:\n");
    printf("%d * %d = %d\n", var1, var2, var1 * var2);

    printf("Divisao:\n");
    printf("%d / %d = %d\n", var1, var2, var1 / var2);

}