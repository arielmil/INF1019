#include <stdio.h>
#include <signal.h> // para signal() 
#include <stdlib.h>
#include <unistd.h> // getpid()

#define EVER ;;

void killHandler(int sinal)  {
    // Nunca será chamado!
    printf("Você pressionou mandou um SIGKILL (%d) \n", sinal);
}

int main(void) {
    printf("Tentarei agora interceptar o sinal SIGKILL.\n");
    printf("Isso nao funcionara, pois SIGKILL e um sinal que nao pode ser interceptado.\n");
    printf("Portanto o que ocorrera quando o programa receber um SIGKILL, sera o comportamento padrao para este sinal (Matar o processo).\n");

    signal(SIGKILL, killHandler); 

    printf("Processo agora entrara em loop até que seja morto com: kill -s SIGKILL %d (ou interrompido)\n", getpid());
    for(EVER);
}