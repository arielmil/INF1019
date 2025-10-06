#include <stdio.h>
#include <stdlib.h> // Para  rand()
#include <signal.h> // Para kill() e signal()
#include <unistd.h> // Para sleep()
#include <sys/types.h> // Para tipos como pid_t
#include <stdlib.h> // Para atoi()
#include <time.h> // Para time()

int main(int argc, char *argv[]) {

    pid_t linhaTelefonica = atoi(argv[1]);
    
    srand((unsigned)time(NULL)); // Para seed
    int tempoDeChamada = (rand() % 10) + 1; // Chamadas duram entre 1 e 11 segundos

    // Chamada inicia
    kill(linhaTelefonica, SIGUSR1);

    sleep(tempoDeChamada);

    // Chamada Termina
    kill(linhaTelefonica, SIGUSR2);

    return 0;
}