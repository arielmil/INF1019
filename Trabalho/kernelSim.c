#include <stdio.h>
#include <stdlib.h> // Para exit e NULL
#include <sys/shm.h> // Para shmget(), shmat(), shmdt() e shmctl()
#include <sys/ipc.h> // Para flags IPC_CREAT, IPC_EXCL, IPC_NOWAIT e estrutura ipc_perm
#include <sys/stat.h> // Para flags de permissão
#include <sys/types.h> // Para tipos como pid_t
#include <fcntl.h> // Para flags de controle de FIFO.
#include <signal.h> // Para tratamento de sinais
#include <unistd.h> // Para pause()
#include <errno.h> // CORREÇÃO: para tratar EAGAIN em leituras non-blocking

#include "info.h"
#include "irq.h"
#include "fila.c"

// Fazer em seguida:

/*

    Quando um process fizer alguma syscall simulada para D1 ou D2, enviar uma mensagem para kernelSim através de um SIGUSR1
    kernelSim então deve olhar para a shmem deste process para verificar qual tipo de operação será feita e para qual dispositivo.
    KernelSim deve então mandar uma mensagem (decidir a melhor forma) para ICS (via fifo) informando qual processo (numero do processo e talvez pid) fez essa requisição
    
    O controle deve ser desviado então para ICS que deve tratar a interrupção adequadamente

    Fazer a parte de interação entre kernelSim e ICS para as interrupções de fatia de tempo (IRQ0)
    
    Passar para kernelSim através de um Pipe, todas as keys de shmem de cada AN, e a key de ICS, para ele conseguir printar as informações pedidas quando interrompido com ctrl-c (Ver se precisa)

    Trocar a lógica de mexer na estrutura info no kernelSim, e o process faz o request,
    ao invés do process em sí mexer direto na estrutura, process apenas faz o request para KS fazer isso.

*/


int lastSignal = -1;

typedef struct processDictionary {
    pid_t pid;
    int number;
} PD;

int getProcessNumber (pid_t pid, PD pd[5]) {

    for (int i = 0; i < 5; i++) {

        if (pd[i].pid == pid) {
            return pd[i].number;
        }
    }

    return -1; // Erro

}

// A fila a seguir é a dos processos prontos para serem posteriormente escalonados.
// Foi uma decisão de projeto utilizada para facilitar o escalonamento, mas não é parte da especificação do trabalho.
Fila ready;

/*
    Espec é: D1 e D2 são setados no ICS com suas probabilidades (P1 é 20 vezes P2), e 

    No AN por exemplo, com uma baixa probabilidade, ele faz uma syscall(Dn, Op) onde Op é uma das 3 operações possíveis acima.

    Essa "syscall" tem que bloquear (parando o processo que solicitou E/S) o AN, e depois, com as interrupções IRQ1 e IRQ2, em ordem de chegada e 
    em ordem FIFO de pedidos de bloqueados, um por um, essa interrupção vai desbloqueando um a um os ANs que estavam esperando E/S, e os ANs desbloqueados 
    depois entram em fila de escalonamento novamente (ready), e a partir da próxima IRQ0 um por um começam a rodar novamente 
    
    Um AN não pode pedir E/S e novamente na próxima salta de tempo tentar rodar. Ele tem que ser desbloqueado por IRQ1 ou IRQ2

    No nosso caso, ainda vamos implementar como sem kernelSim intermediar a solicitação de E/S. Mas já vamos criar um controle de bloqueados no kernelSim 
    para cada um dos dois dispositivos (D1 e D2)
*/

Fila waitingD1; // Fila dos que pediram E/S para D1
Fila waitingD2; // Fila dos que pediram E/S para D2


// Recebe o pid do AN em execução atual
// Retorna 0 caso não existam processos prontos
int updateCurrentProcess(pid_t *currentProcess, Fila *ready) {

    if (!empty(ready)) {

        *currentProcess = pop(ready);

        return 1;
    }

    else {
        *currentProcess = -1;
        return 0;
    }

}

// Atualmente apenas um processo roda por vez.
// O que está aqui é da forma que foi vista em aula (mapeia para SIGSTOP e SIGCONT)
int stopOtherProcessess(pid_t pid, PD pd[5]) {

    int test;
    pid_t pidx;

    for (int i = 1; i <= 5 ; i++) {

        pidx = pd[i - 1].pid;

        if (pid != pidx) {

            test = kill(pidx, SIGSTOP); // Para de rodar os outros processos

            if (test == -1) {
            perror("[KernelSim]: Erro ao mandar um SIGSTOP para algum AN. Saindo...");
            exit(-18);
            }
        } 
        
        else {
            // Não faz nada
        }
    }

    return 0;
}

void trataSinal(int sinal) {

    if (sinal == SIG_IRQ0) // Fatia de tempo
    {
        lastSignal = SIG_IRQ0;
    }

    else if (sinal == SIG_IRQ1) // D1
    {
        lastSignal = SIG_IRQ1;
    }

    else if (sinal == SIG_IRQ2) // D2
    {
        lastSignal = SIG_IRQ2;
    }

    else {
        lastSignal = -1;
    }
}


// KernelSim
int main(int argc, char *argv[]) {

    int test;

    int fifoan1;
    int fifoan2;
    int fifoics;

    char bufferan1; // Vai indicar se o processo mandou \n (não solicitou E/S), e também para qual dispositivo ele quer
    char bufferan2; // Indica qual operação para o dispositivo ele quer (R, W ou X)

    int extendedStop = 0; // indica se deve parar todo mundo (dispositivo) + ele mesmo

    Info *info[5];

    pid_t process[5];

    pid_t pid_kernelSim;

    int processNumberValue;
    Info *currentInfo;

    int maxInvalidTries = 0;

    int pid;
    int number;

    pid_t currentProcess;

    pid_t ICS;

    PD pd[5];

    int shmIdProcess[5];

    // Trata sinais
    signal(SIG_IRQ0, trataSinal);
    signal(SIG_IRQ1, trataSinal);
    signal(SIG_IRQ2, trataSinal);

    // Seta kernelSim pid
    pid_kernelSim = getpid();

    // Faz as FIFOS
    if (mkfifo("FIFOAN1", (S_IRUSR | S_IWUSR)) != 0) {
        perror("[KernelSim]: Erro ao criar a FIFOAN1. Saindo...");
        exit(-5);
    } 

    if ((fifoan1 = open("FIFOAN1", (O_RDONLY | O_NONBLOCK))) < 0) {
        perror("[KernelSim]: Erro ao abrir a FIFOAN1 para leitura. Saindo...");
        exit(-6);
    }

    if (mkfifo("FIFOAN2", (S_IRUSR | S_IWUSR)) != 0) {
        perror("[KernelSim]: Erro ao criar a FIFOAN2. Saindo...");
        exit(-7);
    } 

    if ((fifoan2 = open("FIFOAN2", (O_RDONLY | O_NONBLOCK))) < 0) {
        perror("[KernelSim]: Erro ao abrir a FIFOAN2 para leitura. Saindo...");
        exit(-8);
    }

    if (mkfifo("FIFOICS", (S_IRUSR | S_IWUSR)) != 0) {
        perror("[KernelSim]: Erro ao criar a FIFOICS. Saindo...");
        exit(-9);
    } 

    if ((fifoics = open("FIFOICS", (O_RDONLY | O_NONBLOCK))) < 0) {
        perror("[KernelSim]: Erro ao abrir a FIFOICS para leitura. Saindo...");
        exit(-10);
    }

    // Abre shmem com ICS (vetor com 5 ints para IDs de shmem dos ANs)
    // [CORREÇÃO]: tamanho ajustado para caber 5 IDs de shmem, que o ICS vai ler.
    int shmIdICS;
    shmIdICS = shmget(1234, (size_t)(5 * sizeof(int)), (S_IRUSR | S_IWUSR));
    if (shmIdICS < 0) {
        perror("[KernelSim]: Erro ao abrir shmem de ICS. Saindo...");
        exit(-11);
    }

    int *shmICSptr = (int *) shmat(shmIdICS, NULL, 0);
    if (shmICSptr == (void *) -1) {
        perror("[KernelSim]: Erro ao anexar shmem de ICS. Saindo...");
        exit(-12);
    }

    // [CORREÇÃO]: NÃO vamos mais tentar ler "PID do ICS" aqui,
    // porque o ICS espera nesta SHM um VETOR com 5 IDs de shmem dos ANs.
    // O preenchimento acontece LOGO APÓS abrirmos as SHMs dos ANs, usando shmIdProcess[i].


    ICS = *shmICSptr;

    // Abre shmems dos processos
    for (int i = 0; i < 5; i++) {
        shmIdProcess[i] = shmget((key_t) (4321 + i), (size_t) sizeof(Info), (S_IRUSR | S_IWUSR));
        
        if (shmIdProcess[i] < 0) {
            perror("[KernelSim]: Erro ao abrir shmem de AN. Saindo...");
            exit(-13);
        }

        info[i] = (Info *) shmat(shmIdProcess[i], NULL, 0);
        if (info[i] == (void *) -1) {
            perror("[KernelSim]: Erro ao anexar shmem de AN. Saindo...");
            exit(-14);
        }

        process[i]   = info[i]->pid;
        pd[i].pid    = process[i];
        pd[i].number = info[i]->number;

        // [CORREÇÃO]: disponibiliza ao ICS os IDs de shmem dos 5 ANs
        // para que o ICS faça o briefing lendo diretamente os Info*.
        shmICSptr[i] = shmIdProcess[i];
    }


    // Inicializa filas
    init(&ready);
    init(&waitingD1);
    init(&waitingD2);

    // Coloca todo mundo na fila de prontos inicialmente
    for (int i = 0; i < 5; i++) {
        push(&ready, process[i]);
    }

    // Loop principal
    while (1) {

        // Se não há processos prontos, aguarda uma IRQ que possa desbloquear alguém
        if (!updateCurrentProcess(&currentProcess, &ready)) {
            pause(); // aguarda IRQ
            if (lastSignal == SIG_IRQ1) {
                if (!empty(&waitingD1)) {
                    pid_t acordadoD1 = pop(&waitingD1); // CORREÇÃO: acordado vai para ready (não preempta)
                    push(&ready, acordadoD1);
                }
            }
            else if (lastSignal == SIG_IRQ2) {
                if (!empty(&waitingD2)) {
                    pid_t acordadoD2 = pop(&waitingD2); // CORREÇÃO: idem
                    push(&ready, acordadoD2);
                }
            }
            lastSignal = -1;
            continue;
        }

        // Atualiza info do processo corrente
        processNumberValue = getProcessNumber(currentProcess, pd);
        currentInfo = info[processNumberValue - 1];

        // Coloca apenas o processo atual para rodar
        test = kill(currentProcess, SIGCONT);
        if (test == -1) {
            perror("[KernelSim]: Erro ao mandar SIGCONT. Saindo...");
            exit(-15);
        }

        // Para os demais
        stopOtherProcessess(currentProcess, pd);

        // Lê “pedido de E/S” (se houver) do AN atual
        // FIFOAN1: ‘1’ (D1), ‘2’ (D2), ‘\n’ (sem pedido) — leitura é O_NONBLOCK
        bufferan1 = 0;
        test = read(fifoan1, &bufferan1, 1);
        if (test < 0) {
            // CORREÇÃO: EAGAIN em O_NONBLOCK significa ausência de dados agora; não é erro
            if (errno == EAGAIN) {
                bufferan1 = 0; // sem pedido nesta rodada
            }
            else {
                perror("[KernelSim]: Erro ao ler FIFOAN1. Saindo...");
                exit(-22);
            }
        }

        if (bufferan1 == '1' || bufferan1 == '2') {

            // Le a operação (R/W/X) na FIFOAN2 — também O_NONBLOCK
            bufferan2 = 0;
            test = read(fifoan2, &bufferan2, 1);
            if (test < 0) {
                if (errno == EAGAIN) {
                    // CORREÇÃO: ainda não chegou a operação — trata como ausência de syscall completa
                    bufferan2 = 0;
                }
                else {
                    perror("[KernelSim]: Erro ao ler FIFOAN2. Saindo...");
                    exit(-24);
                }
            }

            if (bufferan2 == 'R' || bufferan2 == 'W' || bufferan2 == 'X') {

                // CORREÇÃO: ao pedir E/S, o AN deve ser parado imediatamente e colocado na fila de bloqueados
                test = kill(currentProcess, SIGSTOP);
                if (test == -1) {
                    perror("[KernelSim]: Erro ao enviar SIGSTOP por syscall. Saindo...");
                    exit(-99);
                }

                // Atualiza estado/contadores mínimos se você estiver guardando no Info (opcional)
                // currentInfo->lastOp = bufferan2; currentInfo->lastDev = bufferan1;

                if (bufferan1 == '1') {
                    push(&waitingD1, currentProcess);
                }
                else {
                    push(&waitingD2, currentProcess);
                }

                // Solta a CPU; quem vai executar será escolhido após próxima IRQ0
                currentProcess = -1;

                // Limpa buffers
                bufferan1 = 0;
                bufferan2 = 0;

                // Volta ao laço para escolher outro pronto (ou aguardar IRQ)
                continue;
            }

            else {
                // CORREÇÃO: sem operação válida -> considera que não houve syscall completa nesta rodada.
                // Segue adiante para tratar IRQs normalmente.
            }
        }
        else {
            // CORREÇÃO: sem syscall nesta rodada — segue fluxo normal
        }

        // Aguarda IRQ (fatia de tempo ou desbloqueio de dispositivo)
        pause();

        if (lastSignal == SIG_IRQ0 || lastSignal == SIG_IRQ1 || lastSignal == SIG_IRQ2) {

            // Se ainda não terminou, trata a causa da interrupção
            if (currentInfo->state != '4') { // '4' = TERMINATED (ICS imprime %c)

                if (lastSignal == SIG_IRQ0) {

                    // CORREÇÃO: incrementa PC somente no fim da fatia de tempo
                    currentInfo->PC = currentInfo->PC + 1;

                    // Preempção do processo que estava rodando
                    test = kill(currentProcess, SIGSTOP);
                    if (test == -1) {
                        perror("[KernelSim]: Erro ao mandar SIGSTOP no IRQ0. Saindo...");
                        exit(-19);
                    }

                    // Se atingiu MAX, marca como terminado e não reencola
                    if (currentInfo->PC >= MAX) {
                        currentInfo->state = '4'; // CORREÇÃO: TERMINATED como caractere (ICS usa %c)
                    }
                    else {
                        // Reencola para round-robin
                        push(&ready, currentProcess);
                    }
                }

                else if (lastSignal == SIG_IRQ1) {
                    // Desbloqueia UM processo de D1 e coloca em ready (não preempta quem está rodando)
                    if (!empty(&waitingD1)) {
                        pid_t acordadoD1 = pop(&waitingD1); // CORREÇÃO: não sobrescrever currentProcess
                        push(&ready, acordadoD1);
                    }
                }

                else { // SIG_IRQ2
                    if (!empty(&waitingD2)) {
                        pid_t acordadoD2 = pop(&waitingD2); // CORREÇÃO: idem
                        push(&ready, acordadoD2);
                    }
                }
            }

            // Limpa a causa tratada
            lastSignal = -1;
        }

        else {
            perror("[KernelSim]: Erro: lastSignal inválido vindo de ICS. Saindo...");
            exit(-25);
        }
    }

    // Nunca chega aqui em operação normal, mas deixo a limpeza por completude
    close(fifoan1);
    close(fifoan2);
    close(fifoics);

    for (int i = 0; i < 5; i++) {
        shmdt(info[i]);
    }

    shmdt(shmICSptr);
    shmctl(shmIdICS, IPC_RMID, NULL);

    // Limpa as filas
    clear(&ready);
    clear(&waitingD1);
    clear(&waitingD2);

    return 0;
}
