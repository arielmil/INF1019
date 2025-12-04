#include <stdio.h>
#include <stdlib.h> // Para exit e NULL
#include <sys/shm.h> // Para shmget(), shmat(), shmdt() e shmctl()
#include <sys/ipc.h> // Para flags IPC_CREAT, IPC_EXCL, IPC_NOWAIT e estrutura ipc_perm
#include <sys/stat.h> // Para flags de permissão
#include <sys/types.h> // Para tipos como pid_t
#include <sys/wait.h> // Para waitpid()
#include <fcntl.h> // Para flags de controle de FIFO.
#include <signal.h> // Para tratamento de sinais
#include <unistd.h> // Para pause(), usleep()
#include <errno.h>

#include <string.h> // Para memset, strncpy, snprintf, memcpy

#include <sys/socket.h>
#include <netinet/in.h>
#include <arpa/inet.h>

#include "info.h"
#include "irq.h"
#include "fila.h"
#include "sfp.h"

#define REPLY_QUEUE_MAX 64

// Flags globais de IRQ, setadas pelos handlers de sinal

static volatile sig_atomic_t irq0Pending = 0;
static volatile sig_atomic_t irq1Pending = 0;
static volatile sig_atomic_t irq2Pending = 0;

// Dicionário PID -> processNumber (1..5)

typedef struct processDictionary {
    pid_t pid;
    int processNumber;
} PD;

int getProcessNumber(pid_t pid, PD *pd) {
    int i;
    for (i = 0; i < 5; i++) {
        PD current = pd[i];
        if (current.pid == pid) {
            return current.processNumber;
        }
    }

    perror("[KernelSim - getProcessNumber]: Erro ao tentar encontrar o processNumber de um processo. Saindo...");
    exit(-21);
}

// Handlers de sinal

void irq0Handler(int signum) {
    (void)signum;
    irq0Pending = 1;
    return;
}

void irq1Handler(int signum) {
    (void)signum;
    irq1Pending = 1;
    return;
}

void irq2Handler(int signum) {
    (void)signum;
    irq2Pending = 1;
    return;
}

// Fila de respostas do SFSS (fileReplyQueue e dirReplyQueue)

typedef struct replyQueue {
    SFPMessage msgs[REPLY_QUEUE_MAX];
    int head;
    int tail;
    int count;
} ReplyQueue;

static void initReplyQueue(ReplyQueue *q) {
    q->head = 0;
    q->tail = 0;
    q->count = 0;
}

static int emptyReplyQueue(ReplyQueue *q) {
    return (q->count == 0);
}

static int pushReply(ReplyQueue *q, SFPMessage *m) {
    if (q->count >= REPLY_QUEUE_MAX) {
        // fila cheia, descarta (poderia tratar melhor)
        return -1;
    }
    q->msgs[q->tail] = *m;
    q->tail = (q->tail + 1) % REPLY_QUEUE_MAX;
    q->count++;
    return 0;
}

static int popReply(ReplyQueue *q, SFPMessage *m) {
    if (q->count == 0) {
        return -1;
    }
    *m = q->msgs[q->head];
    q->head = (q->head + 1) % REPLY_QUEUE_MAX;
    q->count--;
    return 0;
}

// Poll de respostas do SFSS (recvfrom não bloqueante)

static void pollSFSSReplies(int sockfd, ReplyQueue *fileQ, ReplyQueue *dirQ) {
    SFPMessage msg;
    struct sockaddr_in from;
    socklen_t fromlen = sizeof(from);
    ssize_t n;

    // Tenta ler todas as mensagens pendentes
    while (1) {
        errno = 0;
        n = recvfrom(sockfd, &msg, sizeof(msg), MSG_DONTWAIT,
                     (struct sockaddr *)&from, &fromlen);
        if (n < 0) {
            if (errno == EAGAIN || errno == EWOULDBLOCK) {
                // Nenhuma mensagem pendente
                break;
            } else {
                perror("[KernelSim]: Erro em recvfrom ao ler resposta do SFSS. Saindo...");
                exit(-50);
            }
        }

        if (n == 0) {
            // Nada lido (teoricamente não deveria acontecer aqui)
            break;
        }

        // Classifica resposta como de arquivo ou de diretório
        if (msg.hdr.type == SFP_RD_REP || msg.hdr.type == SFP_WR_REP) {
            pushReply(fileQ, &msg);
        } else if (msg.hdr.type == SFP_DC_REP ||
                   msg.hdr.type == SFP_DR_REP ||
                   msg.hdr.type == SFP_DL_REP) {
            pushReply(dirQ, &msg);
        } else {
            fprintf(stderr, "[KernelSim]: Resposta SFSS com tipo desconhecido: %d\n", msg.hdr.type);
        }
    }
}

// main
int main(void) {
    char bufferD, bufferOp;
    char bufferan[5];

    char processNumber[8];
    char shmIdICSString[32];
    char shmIdProcessString[32];
    char pid_kernelSimString[32];

    int terminatedProcessess = 0;
    int fifoan;
    int test;
    int i;
    int shmIdICS;

    int *shmICSptr;
    int shmIdProcess[5];

    pid_t ICS;
    pid_t pid_kernelSim;

    pid_t process[5];

    PD pd[5];

    Info *info[5];

    // Socket UDP para falar com o SFSS
    int sockfd;
    struct sockaddr_in sfss_addr;
    socklen_t sfss_addrlen = sizeof(sfss_addr);

    // Porta fixa do SFSS (ajuste se o enunciado especificar outra)
    int sfss_port = 3999;

    pid_kernelSim = getpid();

    // Sinaliza para ignorar um SIGINT
    signal(SIGINT, SIG_IGN);

    // Deslinka a FIFO para garantir criação segura
    unlink("FIFOAN");

    if (mkfifo("FIFOAN", (S_IRUSR | S_IWUSR)) != 0) {
        perror("[KernelSim]: Erro ao criar a FIFOAN. Saindo...");
        exit(-5);
    } 

    if ((fifoan = open("FIFOAN", (O_RDONLY | O_NONBLOCK))) < 0) {
        perror("[KernelSim]: Erro ao abrir a FIFOAN para leitura. Saindo...");
        exit(-6);
    }

    // Cria shmem para passar para ICS as shmId da Info* de cada process
    shmIdICS = shmget(IPC_PRIVATE, sizeof(int) * 5, IPC_CREAT | IPC_EXCL | S_IRUSR | S_IWUSR);
    if (shmIdICS < 0) {
        perror("[KernelSim]: Erro ao criar região de memória compartilhada para ICS. Saindo...");
        exit(-28);
    }

    shmICSptr = (int *)shmat(shmIdICS, NULL, 0);
    if (shmICSptr == (void *) -1) {
        perror("[KernelSim]: Erro ao anexar região de memória compartilhada para ICS. Saindo...");
        exit(-30);
    }    

    for (i = 0; i < 5; i++) {

        // Cria shmem para se comunicar com os 5 processos filho
        shmIdProcess[i] = shmget(IPC_PRIVATE, sizeof(Info), IPC_CREAT | IPC_EXCL | S_IRUSR | S_IWUSR);
        if (shmIdProcess[i] < 0) {
            perror("[KernelSim]: Erro ao criar região de memória compartilhada para um processo. Saindo...");
            exit(-8);
        }
        
        // Anexa o segmento de memória compartilhada
        info[i] = (Info *) shmat(shmIdProcess[i], NULL, 0);
        if (info[i] == (void *) -1) {
            perror("[KernelSim]: Erro ao anexar segmento de memória compartilhada para um processo. Saindo...");
            shmctl(shmIdProcess[i], IPC_RMID, NULL);
            exit(-20);
        }

        // Escreve na shmem de ICS qual o id de uma das shmem do processo Ai
        shmICSptr[i] = shmIdProcess[i];

        process[i] = fork();
        if (process[i] < 0) {
            perror("[KernelSim]: Erro ao fazer o fork para um processo. Saindo...");
            exit(-9 - i);
        }

        if (process[i] == 0) {
            // Área do filho processo i

            // Converte processNumber (i) e shmId para string
            sprintf(processNumber, "%d", i + 1);
            snprintf(shmIdProcessString, sizeof(shmIdProcessString), "%d", (int)shmIdProcess[i]);

            char *args[] = {"process", processNumber, shmIdProcessString, NULL};
            execvp("./process", args);

            _exit(127);
        }

        // Começa o estado de cada processo AN como parado
        test = kill(process[i], SIGSTOP);
        if (test == -1) {
            perror("[KernelSim]: Erro ao mandar um SIGSTOP para algum AN na hora de criar-lo. Saindo...");
            exit(-17);
        }

        // Inicializa Info[i]
        info[i]->pid = process[i];
        info[i]->state = STOPPED;
        info[i]->lastD = '-';
        info[i]->lastOp = '-';
        info[i]->PC = 0;
        info[i]->timesD1Acessed = 0; // agora: operações de arquivo
        info[i]->timesD2Acessed = 0; // agora: operações de diretório
        info[i]->fs_kind = 0;
        info[i]->fs_op = 0;
        info[i]->fs_path[0] = '\0';
        info[i]->fs_offset = 0;
        memset(info[i]->fs_payload, 0, FS_PAYLOAD_SIZE);
        info[i]->fs_name[0] = '\0';

        // Monta uma das 5 entradas para o dicionário de PID para numero de processo [1...5]
        pd[i].pid = process[i];
        pd[i].processNumber = i + 1;
    }

    ICS = fork();
    if (ICS < 0) {
        perror("[KernelSim]: Erro ao fazer o fork para o ICS. Saindo...");
        exit(-7);
    }

    if (ICS == 0) {
        // Área do filho ICS

        snprintf(shmIdICSString, sizeof(shmIdICSString), "%ld", (long)shmIdICS);
        snprintf(pid_kernelSimString, sizeof(pid_kernelSimString), "%d", (int)pid_kernelSim);

        char *args[] = {"interruptionControllerSim", pid_kernelSimString, shmIdICSString, NULL};
        execvp("./interruptionControllerSim", args);

        _exit(127);
    }

    // Deixa ICS em estado parado até o escalonamento começar
    test = kill(ICS, SIGSTOP);
    if (test == -1) {
        perror("[KernelSim]: Erro ao enviar sinal SIGSTOP para ICS. Saindo...");
        exit(-26);
    }

    // A partir daqui apenas o pai roda, pois todos os filhos estão executando cada um seu código
    signal(IRQ0, irq0Handler);
    signal(IRQ1, irq1Handler);
    signal(IRQ2, irq2Handler);

    // Inicializa socket UDP cliente para falar com SFSS
    sockfd = socket(AF_INET, SOCK_DGRAM, 0);
    if (sockfd < 0) {
        perror("[KernelSim]: Erro ao abrir socket UDP para SFSS. Saindo...");
        exit(-60);
    }

    memset(&sfss_addr, 0, sizeof(sfss_addr));
    sfss_addr.sin_family = AF_INET;
    sfss_addr.sin_port = htons((unsigned short)sfss_port);
    // SFSS rodando na mesma máquina (localhost)
    sfss_addr.sin_addr.s_addr = htonl(INADDR_LOOPBACK);

    // Filas de escalonamento e de espera
    Fila ready;
    Fila waitingFile; // processos esperando operação de arquivo
    Fila waitingDir;  // processos esperando operação de diretório

    init(&ready);
    init(&waitingFile);
    init(&waitingDir);

    // Filas de respostas do SFSS
    ReplyQueue fileReplyQueue;
    ReplyQueue dirReplyQueue;
    initReplyQueue(&fileReplyQueue);
    initReplyQueue(&dirReplyQueue);

    // Faz ICS voltar a continuar
    test = kill(ICS, SIGCONT);
    if (test == -1) {
        perror("[KernelSim]: Erro ao enviar sinal SIGSCONT para ICS. Saindo...");
        exit(-27);
    }

    // Coloca os 5 processos AN na fila de prontos
    for (i = 0; i < 5; i++) {
        push(&ready, process[i]);
        info[i]->state = READY;
    }

    // A partir daqui, escalona
    pid_t currentProcess;

    Info *currentInfo;
    Info *syscalledInfo;

    while (terminatedProcessess < 5) {

        // 9.1.1: enquanto ready estiver vazia, trata apenas IRQ1/IRQ2 + respostas SFSS
        while (empty(&ready)) {

            // 9.1.1.1: checagem não bloqueante por respostas do SFSS
            pollSFSSReplies(sockfd, &fileReplyQueue, &dirReplyQueue);

            // 9.1.1.2: trata IRQ1 / IRQ2, entregando respostas aos processos
            if (irq1Pending) {

                if (!emptyReplyQueue(&fileReplyQueue)) {

                    SFPMessage reply;

                    if (popReply(&fileReplyQueue, &reply) == 0) {

                        int owner = reply.hdr.owner; // 1..5
                        syscalledInfo = info[owner - 1];

                        // Copia dados de retorno de arquivo para Info[owner]
                        if (reply.hdr.type == SFP_RD_REP) {

                            // payload lido e offset de retorno
                            memcpy(syscalledInfo->fs_payload, reply.rd_rep.payload, FS_PAYLOAD_SIZE);
                            syscalledInfo->fs_offset = reply.rd_rep.offset;

                        } 
                        
                        else if (reply.hdr.type == SFP_WR_REP) {
                            // poderíamos guardar novo EOF em fs_offset
                            syscalledInfo->fs_offset = reply.wr_rep.offset;
                        }

                        if (syscalledInfo->state == WAITING_FILE) {
                            syscalledInfo->state = READY;
                            push(&ready, syscalledInfo->pid);
                        }

                    }
                }

                irq1Pending = 0;
            }

            if (irq2Pending) {
                
                if (!emptyReplyQueue(&dirReplyQueue)) {
                    
                    SFPMessage reply;
                    
                    if (popReply(&dirReplyQueue, &reply) == 0) {
                        int owner = reply.hdr.owner; // 1..5
                        syscalledInfo = info[owner - 1];

                        // Copiar informações específicas de diretório se necessário
                        // (ex.: nrnames, path, etc) - aqui não usamos.

                        if (syscalledInfo->state == WAITING_DIR) {
                            syscalledInfo->state = READY;
                            push(&ready, syscalledInfo->pid);
                        }

                    }
                }
                irq2Pending = 0;
            }

            // 9.1.1.3: pequeno sleep para evitar busy wait
            usleep(100000);
        }

        // 9.1.2: há alguém pronto
        currentProcess = pop(&ready);
        currentInfo = info[getProcessNumber(currentProcess, pd) - 1];

        currentInfo->state = RUNNING;

        test = kill(currentProcess, SIGCONT);
        if (test == -1) {
            perror("[KernelSim]: Erro ao enviar um SIGCONT para um processo. Saindo...");
            exit(-36);
        }

        // 9.1.4: enquanto não receber syscall nem IRQ, fica nesse loop
        while (!irq0Pending && !irq1Pending && !irq2Pending) {

            // 9.1.4.1: pequena pausa
            usleep(100000);

            // 9.1.4.2: checagem não bloqueante por respostas do SFSS
            pollSFSSReplies(sockfd, &fileReplyQueue, &dirReplyQueue);

            // 9.1.4.3: tenta ler da FIFOAN (syscall de FS do currentProcess)
            errno = 0;
            test = read(fifoan, bufferan, 5);

            if (test <= 0) {
                
                if (errno == EAGAIN || errno == EWOULDBLOCK || test == 0) {
                    // Nenhuma syscall nessa iteração
                    continue;
                }

                perror("[KernelSim]: Erro ao tentar dar read na FIFOAN. Saindo...");
                exit(-37);

            }

            // Se chegou aqui, recebeu mensagem de syscall (5 bytes)
            bufferD = bufferan[0]; // 'F' ou 'D'
            bufferOp = bufferan[3]; // 'R','W','A','M','L'

            // Atualiza info do processo com ultimo dispositivo/operação
            currentInfo->lastD = bufferD;
            currentInfo->lastOp = bufferOp;

            if (bufferD == FS_KIND_FILE) {
                // Operação de arquivo: read/write

                currentInfo->timesD1Acessed++; // contador de operações de arquivo
                currentInfo->state = WAITING_FILE;

                // Monta mensagem SFP de arquivo
                SFPMessage req;
                memset(&req, 0, sizeof(req));

                if (bufferOp == FS_OP_READ) {
                    req.rd_req.hdr.type  = SFP_RD_REQ;
                    req.rd_req.hdr.owner = getProcessNumber(currentProcess, pd); // 1..5

                    strncpy(req.rd_req.path, currentInfo->fs_path, SFP_PATH_MAX);
                    req.rd_req.path[SFP_PATH_MAX - 1] = '\0';
                    req.rd_req.strlen_path = (int)strlen(req.rd_req.path);

                    memset(req.rd_req.payload, 0, SFP_PAYLOAD_SIZE);
                    req.rd_req.offset = currentInfo->fs_offset;

                    if (sendto(sockfd, &req.rd_req, sizeof(req.rd_req), 0,
                               (struct sockaddr *)&sfss_addr, sfss_addrlen) < 0) {
                        perror("[KernelSim]: Erro em sendto (RD-REQ). Saindo...");
                        exit(-61);
                    }

                } 
                
                else if (bufferOp == FS_OP_WRITE) {
                    req.wr_req.hdr.type  = SFP_WR_REQ;
                    req.wr_req.hdr.owner = getProcessNumber(currentProcess, pd); // 1..5

                    strncpy(req.wr_req.path, currentInfo->fs_path, SFP_PATH_MAX);
                    req.wr_req.path[SFP_PATH_MAX - 1] = '\0';
                    req.wr_req.strlen_path = (int)strlen(req.wr_req.path);

                    memcpy(req.wr_req.payload, currentInfo->fs_payload, FS_PAYLOAD_SIZE);
                    req.wr_req.offset = currentInfo->fs_offset;

                    if (sendto(sockfd, &req.wr_req, sizeof(req.wr_req), 0,
                               (struct sockaddr *)&sfss_addr, sfss_addrlen) < 0) {
                        perror("[KernelSim]: Erro em sendto (WR-REQ). Saindo...");
                        exit(-62);
                    }


                } 
                
                else {
                    fprintf(stderr, "[KernelSim]: Operação de arquivo desconhecida: %c\n", bufferOp);
                    exit(-63);
                }

                // Coloca processo em waitingFile e para sua execução
                push(&waitingFile, currentProcess);

                test = kill(currentProcess, SIGSTOP);
                if (test == -1) {
                    perror("[KernelSim]: Erro ao enviar um SIGSTOP para um processo (FILE). Saindo...");
                    exit(-41);
                }

                // Sai do while por ter feito syscall
                break;
            }

            else if (bufferD == FS_KIND_DIR) {
                // Operação de diretório: add/rem/listdir

                currentInfo->timesD2Acessed++; // contador de operações de diretório
                currentInfo->state = WAITING_DIR;

                SFPMessage req;
                memset(&req, 0, sizeof(req));

                if (bufferOp == FS_OP_ADD) {
                    req.dc_req.hdr.type  = SFP_DC_REQ;
                    req.dc_req.hdr.owner = getProcessNumber(currentProcess, pd);

                    strncpy(req.dc_req.path, currentInfo->fs_path, SFP_PATH_MAX);
                    req.dc_req.path[SFP_PATH_MAX - 1] = '\0';
                    req.dc_req.strlen_path = (int)strlen(req.dc_req.path);

                    strncpy(req.dc_req.dirname, currentInfo->fs_name, SFP_NAME_MAX);
                    req.dc_req.dirname[SFP_NAME_MAX - 1] = '\0';
                    req.dc_req.strlen_dirname = (int)strlen(req.dc_req.dirname);

                    if (sendto(sockfd, &req.dc_req, sizeof(req.dc_req), 0,
                               (struct sockaddr *)&sfss_addr, sfss_addrlen) < 0) {
                        perror("[KernelSim]: Erro em sendto (DC-REQ). Saindo...");
                        exit(-64);
                    }

                } 
                
                else if (bufferOp == FS_OP_REM) {
                    req.dr_req.hdr.type  = SFP_DR_REQ;
                    req.dr_req.hdr.owner = getProcessNumber(currentProcess, pd);

                    strncpy(req.dr_req.path, currentInfo->fs_path, SFP_PATH_MAX);
                    req.dr_req.path[SFP_PATH_MAX - 1] = '\0';
                    req.dr_req.strlen_path = (int)strlen(req.dr_req.path);

                    strncpy(req.dr_req.name, currentInfo->fs_name, SFP_NAME_MAX);
                    req.dr_req.name[SFP_NAME_MAX - 1] = '\0';
                    req.dr_req.strlen_name = (int)strlen(req.dr_req.name);

                    if (sendto(sockfd, &req.dr_req, sizeof(req.dr_req), 0,
                               (struct sockaddr *)&sfss_addr, sfss_addrlen) < 0) {
                        perror("[KernelSim]: Erro em sendto (DR-REQ). Saindo...");
                        exit(-65);
                    }

                } 
                
                else if (bufferOp == FS_OP_LIST) {
                    req.dl_req.hdr.type  = SFP_DL_REQ;
                    req.dl_req.hdr.owner = getProcessNumber(currentProcess, pd);

                    strncpy(req.dl_req.path, currentInfo->fs_path, SFP_PATH_MAX);
                    req.dl_req.path[SFP_PATH_MAX - 1] = '\0';
                    req.dl_req.strlen_path = (int)strlen(req.dl_req.path);

                    if (sendto(sockfd, &req.dl_req, sizeof(req.dl_req), 0,
                               (struct sockaddr *)&sfss_addr, sfss_addrlen) < 0) {
                        perror("[KernelSim]: Erro em sendto (DL-REQ). Saindo...");
                        exit(-66);
                    }
                } 
                
                else {
                    fprintf(stderr, "[KernelSim]: Operação de diretório desconhecida: %c\n", bufferOp);
                    exit(-67);
                }

                push(&waitingDir, currentProcess);

                test = kill(currentProcess, SIGSTOP);
                if (test == -1) {
                    perror("[KernelSim]: Erro ao enviar um SIGSTOP para um processo (DIR). Saindo...");
                    exit(-39);
                }

                // Sai do while por ter feito syscall
                break;
            }

            else {
                perror("[KernelSim]: Erro: opção inválida para tipo de operação (nem FILE nem DIR). Saindo...");
                exit(-38);
            }
        } // fim while interno (9.1.4)

        // 9.1.5: saiu do while por syscall ou IRQ
        if (currentInfo->PC >= MAX) {
            currentInfo->state = TERMINATED;
            terminatedProcessess++;
        }

        // 9.1.5.2: Trata IRQ0 (fim de quantum)
        if (irq0Pending) {
            if (currentInfo->state == RUNNING) {

                test = kill(currentProcess, SIGSTOP);
                if (test == -1) {
                    perror("[KernelSim]: Erro ao enviar um SIGSTOP para um processo (IRQ0). Saindo...");
                    exit(-40);
                }

                currentInfo->state = STOPPED;
            }

            irq0Pending = 0;
        }

        // 9.1.5.3: Trata IRQ1 (entrega de respostas de arquivo)
        if (irq1Pending) {

            if (!emptyReplyQueue(&fileReplyQueue)) {

                SFPMessage reply;

                if (popReply(&fileReplyQueue, &reply) == 0) {
                    int owner = reply.hdr.owner;
                    syscalledInfo = info[owner - 1];

                    if (reply.hdr.type == SFP_RD_REP) {
                        memcpy(syscalledInfo->fs_payload, reply.rd_rep.payload, FS_PAYLOAD_SIZE);
                        syscalledInfo->fs_offset = reply.rd_rep.offset;
                    } 
                    
                    else if (reply.hdr.type == SFP_WR_REP) {
                        syscalledInfo->fs_offset = reply.wr_rep.offset;
                    }

                    if (syscalledInfo->state == WAITING_FILE) {
                        syscalledInfo->state = READY;
                        push(&ready, syscalledInfo->pid);
                    }

                }

            }
            irq1Pending = 0;
        }

        // 9.1.5.4: Trata IRQ2 (entrega de respostas de diretório)
        if (irq2Pending) {
            
            if (!emptyReplyQueue(&dirReplyQueue)) {
                
                SFPMessage reply;
                
                if (popReply(&dirReplyQueue, &reply) == 0) {
                    int owner = reply.hdr.owner;
                    syscalledInfo = info[owner - 1];

                    // (Copiar campos de diretório, se desejado)

                    if (syscalledInfo->state == WAITING_DIR) {
                        syscalledInfo->state = READY;
                        push(&ready, syscalledInfo->pid);
                    }
                }

            }
            irq2Pending = 0;
        }

        // 9.1.5.5: Se processo apenas parou por IRQ0 (STOPPED) e não TERMINATED, volta pra ready
        if (currentInfo->state != TERMINATED && currentInfo->state == STOPPED) {
            push(&ready, currentProcess);
            currentInfo->state = READY;
        }
    }

    // Fecha tudo e sai
    printf("\nTodos os processos acabaram de rodar. Encerrando...\n");

    // Fecha e unlinka a FIFO
    close(fifoan);
    unlink("FIFOAN");

    // Fecha socket UDP
    close(sockfd);

    // Detacha e deleta as shm
    for (i = 0; i < 5; i++) {
        shmdt(info[i]);
        shmctl(shmIdProcess[i], IPC_RMID, NULL);
    }

    shmdt(shmICSptr);
    shmctl(shmIdICS, IPC_RMID, NULL);

    // Encerra os 5 processos AN
    for (i = 0; i < 5; i++) {
        test = kill(process[i], SIGTERM);
        if (test == -1) {
            perror("[KernelSim]: Erro ao tentar encerrar algum processo. Saindo...");
            exit(-34);
        }
        waitpid(process[i], NULL, 0);
    }

    // Encerra o ICS
    test = kill(ICS, SIGTERM);
    if (test == -1) {
        perror("[KernelSim]: Erro ao tentar encerrar ICS. Saindo...");
        exit(-35);
    }
    waitpid(ICS, NULL, 0);

    // Limpa as filas (de PIDs)
    clear(&ready);
    clear(&waitingFile);
    clear(&waitingDir);

    return 0;
}
