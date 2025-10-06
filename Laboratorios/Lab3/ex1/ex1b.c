#include <stdio.h>
#include <signal.h>
#include <stdlib.h>
#define EVER ;;

void intHandler(int sinal);
void quitHandler(int sinal);

int main (void) {
    
    puts ("Este programa é uma versão do ex1a (ctrl-c) sem os handlers.");
    printf("O que se espera acontecer: Com a retirada das funcoes signal, os sinais SIGINT e SIGQUIT nao serao tratados por este processo\n");
    printf("Logo, os tratamentos de ambos os sinais serao os tratamentos padroes herdados do kernel.\n");
    
    for(EVER);
}

void intHandler(int sinal)  {
    printf("Você pressionou Ctrl-C (%d) \n", sinal);
}

void quitHandler(int sinal) {
    printf("Terminando o processo...\n");
    exit (0);
}
