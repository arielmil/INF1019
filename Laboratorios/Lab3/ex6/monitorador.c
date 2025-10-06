#include <stdio.h>
#include <signal.h> // Para signal()
#include <unistd.h> // Para sleep()
#include <stdlib.h> // Para exit()

// Variavel global para que os handlers e funcoes chamadas por eles possam alterar este valor
int tempoDeChamada = 0;


// Aqui (para fins de testes) a regra foi alterada para:

// Chamadas de 5 segundos ou menos custam 2 centavos por segundo
// Chamadas de mais de 5 segundos custam 1 centavo por cada segundo adicional
int calculaCusto() {
    if (tempoDeChamada <= 5) {
        return 2*tempoDeChamada;
    }

    return (10 + tempoDeChamada - 5);
}

void iniciarChamada(int signo) {
    while(1) {
        sleep(1);
        tempoDeChamada++;
    }
}

void terminarChamada(int signo) {

    printf("Chamada durou %d segundo. Custo da chamada: %d centavos.\n", tempoDeChamada, calculaCusto());
    exit(0);
}

int main(void) {
    signal(SIGUSR1, iniciarChamada);
    signal(SIGUSR2, terminarChamada);

    while(1) {
        // Busy waiting para o programa não terminar.
    }
}