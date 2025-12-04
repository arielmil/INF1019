#include <stdio.h>
#include <stdlib.h> // Para exit e NULL
#include <unistd.h> // Para getpid() e usleep()
#include <time.h>   // Para time()
#include <fcntl.h>  // Para flags de controle de FIFO.
#include <signal.h> // Para signal()
#include <string.h> // Para strcpy(), strncpy(), snprintf()
#include <sys/shm.h> // Para shmget(), shmat(), shmdt() e shmctl()
#include "info.h"

// OBS: argv[0]: ./process, argv[1]: processNumber, argv[2]: shmIdProcess

// Gera um offset aleatório em {0,16,32,48,64,80,96}
static int geraOffsetAleatorio(void) {
    int k = rand() % 7; // 0..6
    return k * 16;
}

// Gera um path de arquivo relativo, tipo "dirX/fileY"
static void geraPathArquivo(char *dest, size_t maxLen) {
    int dirId  = (rand() % 3) + 1; // 1..3
    int fileId = (rand() % 5) + 1; // 1..5

    snprintf(dest, maxLen, "dir%d/file%d", dirId, fileId);
    dest[maxLen - 1] = '\0';
}

// Gera um path de diretório relativo, tipo "dirX/subY"
static void geraPathDiretorio(char *dest, size_t maxLen) {
    int dirId  = (rand() % 3) + 1; // 1..3
    int subId  = (rand() % 3) + 1; // 1..3

    snprintf(dest, maxLen, "dir%d/sub%d", dirId, subId);
    dest[maxLen - 1] = '\0';
}

// Gera um nome simples, tipo "nameK"
static void geraNome(char *dest, size_t maxLen) {
    int id = (rand() % 10) + 1; // 1..10
    snprintf(dest, maxLen, "name%d", id);
    dest[maxLen - 1] = '\0';
}

// Preenche payload com 16 bytes aleatórios (letras maiúsculas)
static void geraPayload(char *payload) {
    for (int i = 0; i < FS_PAYLOAD_SIZE; i++) {
        payload[i] = 'A' + (rand() % 26);
    }
}

int main(int argc, char *argv[]) {
    char buffer[] = "D, O";  // mesmo formato do T1: 5 bytes com '\0' no final
    char *mensagem;
    int d;
    int fifoan;
    int shmIdProcess;
    int processNumber; // [1..5]
    int test;

    Info *info;

    pid_t pid = getpid();

    // Sinaliza para ignorar um SIGINT
    signal(SIGINT, SIG_IGN);

    // Caso o Kernel feche a FIFO antes do processo encerrar  
    signal(SIGPIPE, SIG_IGN);
    
    if (argc < 3) {
        perror("Erro: Processo não recebeu o número de argumentos necessários. Saindo...\n");
        _exit(-1);
    }

    processNumber = atoi(argv[1]);
    shmIdProcess  = atoi(argv[2]);

    // 1: Dorme por 0.5s (antes de começar a rodar de fato)
    usleep(500000);

    // 2: Dá shmat na sua própria Info
    info = (Info *)shmat(shmIdProcess, NULL, 0);
    if (info == (void *) -1) {
        perror("[Process]: Erro ao usar shmat. Saindo...");
        _exit(-41);
    }

    // 3: Seeda a função rand
    srand((unsigned)(pid ^ time(NULL)));

    // 4: Abre FIFOAN em modo escrita (para enviar syscalls ao kernelSim)
    fifoan = open("FIFOAN", O_WRONLY);
    if (fifoan < 0) {
        perror("Erro ao abrir FIFOAN para escrita. Saindo...\n");
        _exit(-2);
    }

    // Buffer de mensagem "D, O"
    mensagem = (char *)malloc(sizeof(char) * 5);
    if (mensagem == NULL) {
        perror("[Process]: Erro ao usar malloc para mensagem. Saindo.");
        _exit(-40);
    }
    strcpy(mensagem, buffer);

    // 5: Entra num while(Info->PC < MAX)
    while (info->PC < MAX) {
        
        // 1) CASO ESPECIAL: primeira iteração -> ADD diretório "teste" em "."
        if (info->PC == 0) {
            // Preenche Info com os parâmetros da syscall de diretório ADD
            info->fs_kind = FS_KIND_DIR;      // 'D'
            info->fs_op   = FS_OP_ADD;        // 'A'
            strcpy(info->fs_path, ".");       // pai lógico
            strcpy(info->fs_name, "teste");   // dirname
            info->fs_offset = 0;
            memset(info->fs_payload, 0, FS_PAYLOAD_SIZE);

            // Monta mensagem "D, A" para a FIFO (mesmo formato do resto do código)
            mensagem[0] = FS_KIND_DIR;  // 'D'
            mensagem[1] = ',';          
            mensagem[2] = ' ';
            mensagem[3] = FS_OP_ADD;    // 'A'
            mensagem[4] = '\0';

            test = write(fifoan, mensagem, 5);
            if (test != 5) {
                perror("[Process]: Erro ao usar write em FIFOAN (ADD teste). Saindo...");
                _exit(-50);
            }

            printf("[Processo %d]: syscall DIR ADD em \".\" criando \"teste\".\n",
                processNumber);

            info->PC++;
            usleep(500000);
            continue;  // pula o resto do loop (não sorteia d)
        }

        // 2) CASO ESPECIAL: segunda iteração -> WRITE em "teste/file0"
        if (info->PC == 1) {
            // Preenche Info com os parâmetros da syscall de arquivo WRITE
            info->fs_kind   = FS_KIND_FILE;        // 'F'
            info->fs_op     = FS_OP_WRITE;         // 'W'
            strcpy(info->fs_path, "teste/file0");  // path lógico
            info->fs_offset = 0;                   // começo do arquivo
            geraPayload(info->fs_payload);         // 16 bytes aleatórios

            // Monta mensagem "F, W" para a FIFO
            mensagem[0] = FS_KIND_FILE;  // 'F'
            mensagem[1] = ',';
            mensagem[2] = ' ';
            mensagem[3] = FS_OP_WRITE;   // 'W'
            mensagem[4] = '\0';

            test = write(fifoan, mensagem, 5);
            if (test != 5) {
                perror("[Process]: Erro ao usar write em FIFOAN (WRITE teste/file0). Saindo...");
                _exit(-51);
            }

            printf("[Processo %d]: syscall FILE WRITE em \"teste/file0\", offset 0.\n",
                processNumber);

            info->PC++;
            usleep(500000);
            continue;  // pula sorteio de d
        }
        
        // 5.1: Dorme por 0.5s
        usleep(500000);

        // 5.2: Tira uma probabilidade randômica (1 a 100)
        d = (rand() % 100) + 1;
        printf("\nValor de d: %d em A%d (pid: %ld)\n", d, processNumber, (long) pid);

        // 5.3: Se d < 15 (15% de chance), decide fazer uma syscall de sistema de arquivos
        if (d < 15) {
            // 5.3.1: Decide se a operação será de ARQUIVO ou DIRETÓRIO
            // (exemplo: d par -> arquivo, d ímpar -> diretório)
            int ehArquivo = (d % 2 == 0);

            if (ehArquivo) {
                // 5.3.2: Operação de ARQUIVO

                char path[FS_MAX_PATH];
                int offset;

                // Gera path "dirX/fileY" e offset em {0,16,...,96}
                geraPathArquivo(path, sizeof(path));
                offset = geraOffsetAleatorio();

                // Usa um segundo rand só para escolher entre read/write
                int opEscolha = rand() % 2; // 0 -> read, 1 -> write

                // Preenche Info com os parâmetros da syscall
                info->fs_kind = FS_KIND_FILE;   // 'F'
                info->fs_offset = offset;
                strncpy(info->fs_path, path, FS_MAX_PATH);
                info->fs_path[FS_MAX_PATH - 1] = '\0';

                if (opEscolha == 0) {
                    // read
                    info->fs_op = FS_OP_READ;   // 'R'
                    memset(info->fs_payload, 0, FS_PAYLOAD_SIZE);

                    printf("[Processo %d]: syscall FILE READ em \"%s\", offset %d.\n",
                           processNumber, path, offset);

                    // Na FIFO: 'F' (file), 'R' (read)
                    mensagem[0] = FS_KIND_FILE;
                    mensagem[3] = FS_OP_READ;
                } 
                
                else {
                    // write
                    info->fs_op = FS_OP_WRITE;  // 'W'
                    geraPayload(info->fs_payload);

                    printf("[Processo %d]: syscall FILE WRITE em \"%s\", offset %d.\n",
                           processNumber, path, offset);

                    // Na FIFO: 'F' (file), 'W' (write)
                    mensagem[0] = FS_KIND_FILE;
                    mensagem[3] = FS_OP_WRITE;
                }

                // Escreve na FIFOAN avisando o kernelSim
                test = write(fifoan, mensagem, 5);
                if (test != 5) {
                    perror("[Process]: Erro ao usar write em FIFOAN. Saindo...");
                    _exit(-42);
                }

                // Dorme 0.5s para dar tempo de kernelSim ler a syscall
                usleep(500000);
            }

            else {
                // 5.3.3: Operação de DIRETÓRIO

                char path[FS_MAX_PATH];
                char name[FS_MAX_NAME];

                // Decide entre add, rem ou listdir usando um dDiretorio % 3
                int dDiretorio = rand() % 3; // 0 -> add, 1 -> rem, 2 -> listdir

                // Gera path relativo (ex.: "dirX/subY")
                geraPathDiretorio(path, sizeof(path));
                // Gera nome quando necessário
                geraNome(name, sizeof(name));

                info->fs_kind = FS_KIND_DIR;   // 'D'
                strncpy(info->fs_path, path, FS_MAX_PATH);
                info->fs_path[FS_MAX_PATH - 1] = '\0';

                if (dDiretorio == 0) {
                    // add
                    info->fs_op = FS_OP_ADD;   // 'A'
                    strncpy(info->fs_name, name, FS_MAX_NAME);
                    info->fs_name[FS_MAX_NAME - 1] = '\0';

                    printf("[Processo %d]: syscall DIR ADD em \"%s\" (dirname=\"%s\").\n",
                           processNumber, path, name);

                    mensagem[0] = FS_KIND_DIR; // 'D'
                    mensagem[3] = FS_OP_ADD;   // 'A'
                }

                else if (dDiretorio == 1) {
                    // rem
                    info->fs_op = FS_OP_REM;   // 'M'
                    strncpy(info->fs_name, name, FS_MAX_NAME);
                    info->fs_name[FS_MAX_NAME - 1] = '\0';

                    printf("[Processo %d]: syscall DIR REM em \"%s\" (nome=\"%s\").\n",
                           processNumber, path, name);

                    mensagem[0] = FS_KIND_DIR; // 'D'
                    mensagem[3] = FS_OP_REM;   // 'M'
                }

                else {
                    // listdir
                    info->fs_op = FS_OP_LIST;  // 'L'
                    info->fs_name[0] = '\0';   // não usado para listdir

                    printf("[Processo %d]: syscall DIR LISTDIR em \"%s\".\n",
                           processNumber, path);

                    mensagem[0] = FS_KIND_DIR; // 'D'
                    mensagem[3] = FS_OP_LIST;  // 'L'
                }

                // Escreve na FIFOAN avisando o kernelSim
                test = write(fifoan, mensagem, 5);
                if (test != 5) {
                    perror("[Process]: Erro ao usar write em FIFOAN. Saindo...");
                    _exit(-43);
                }

                // Dorme 0.5s
                usleep(500000);
            }
        }

        else {
            // Não fez syscall nesta iteração
            printf("\n[Processo %d]: Esperando seu tempo de rodar acabar.\n", processNumber);
        }

        // 5.4: Faz Info->PC++
        info->PC++;

        // Pequena pausa extra, igual no T1
        usleep(500000);
    }

    // 6: Ao sair do while, fecha FIFOAN
    printf("\nProcesso %d acabou de rodar.\n", processNumber);

    close(fifoan);

    // Desanexa a shmem
    shmdt(info);

    // 7: _exit(0)
    _exit(0);
}