/*
 * sfss.c - Simple File System Server
 *
 * Como usar: sfss <port> <rootdir>
 *
 * Exemplo:
 *   ./sfss 3999 SFSS-root
 *
 * O servidor aguarda mensagens SFP (Simple File Protocol)
 * vindas do kernelSim (cliente UDP), interpreta o tipo
 * (RD-REQ, WR-REQ, etc.) e responde com xx-REP.
 */

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>

#include <sys/types.h>
#include <sys/socket.h>
#include <sys/stat.h>
#include <fcntl.h>
#include <dirent.h>

#include <netinet/in.h>
#include <arpa/inet.h>

#include "sfp.h"   // Meu protocolo (SFPMessage, SFP_RD_REQ, etc.)

// Função de erro simples (mesmo estilo do udpserver.c)
static void error(const char *msg) {
    perror(msg);
    exit(1);
}

// Monta caminho físico completo a partir de rootdir, owner e path relativo.

// buffer: onde o caminho completo será escrito
// buflen: tamanho total de buffer
// rootdir: diretório raiz (ex.: "SFSS-root")
// owner: número do processo Ax (1..5)
// relpath: path relativo (ex.: "dir1/file2", ".", "")

// Convenções:
//   - Se relpath for vazio ou ".", o caminho é "<rootdir>/A<owner>"
//   - Caso contrário, "<rootdir>/A<owner>/<relpath>"

static void monta_caminho_fisico(char *buffer, size_t buflen, const char *rootdir, int owner, const char *relpath) {
    
    if (relpath == NULL || relpath[0] == '\0' || strcmp(relpath, ".") == 0) {
        snprintf(buffer, buflen, "%s/A%d", rootdir, owner);
    } 
    
    else {
        snprintf(buffer, buflen, "%s/A%d/%s", rootdir, owner, relpath);
    }

    buffer[buflen - 1] = '\0';
}

// Trata RD-REQ (leitura de arquivo)
static void trata_rd_req(int sockfd,SFPReadReq *req,
                         struct sockaddr_in *clientaddr,
                         socklen_t clientlen,
                         const char *rootdir) {
    SFPReadRep rep;
    char fullpath[512];
    int fd;
    ssize_t n;

    memset(&rep, 0, sizeof(rep));

    rep.hdr.type  = SFP_RD_REP;
    rep.hdr.owner = req->hdr.owner;

    // Copia path lógico de volta
    strncpy(rep.path, req->path, SFP_PATH_MAX);
    rep.path[SFP_PATH_MAX - 1] = '\0';
    rep.strlen_path = req->strlen_path;

    monta_caminho_fisico(fullpath, sizeof(fullpath), rootdir, req->hdr.owner, req->path);

    // Abre o arquivo para leitura
    fd = open(fullpath, O_RDONLY);

    if (fd < 0) {
        // Erro ao abrir
        rep.offset = -1;
    } 
    
    else {
        // Vai até o offset solicitado
        off_t off = lseek(fd, req->offset, SEEK_SET);

        if (off < 0) {
            rep.offset = -2; // erro em lseek
        } 
        
        else {
            // Zera payload antes de ler
            memset(rep.payload, 0, SFP_PAYLOAD_SIZE);

            n = read(fd, rep.payload, SFP_PAYLOAD_SIZE);

            if (n < 0) {
                rep.offset = -3; // erro em read
            } 
            
            else {

                // Sucesso (mesmo que n < 16, rep.payload fica com n bytes lidos)
                rep.offset = req->offset;
            }
        }

        close(fd);
    }

    // Envia RD-REP
    if (sendto(sockfd, &rep, sizeof(rep), 0, (struct sockaddr *) clientaddr, clientlen) < 0) {
        error("ERROR in sendto (RD-REP)");
    }
}

// Trata WR-REQ (escrita de arquivo)
static void trata_wr_req(int sockfd,
                         SFPWriteReq *req,
                         struct sockaddr_in *clientaddr,
                         socklen_t clientlen,
                         const char *rootdir) {
    SFPWriteRep rep;
    char fullpath[512];
    int fd;
    ssize_t n = 0;
    off_t off = 0;

    memset(&rep, 0, sizeof(rep));

    rep.hdr.type  = SFP_WR_REP;
    rep.hdr.owner = req->hdr.owner;

    strncpy(rep.path, req->path, SFP_PATH_MAX);
    rep.path[SFP_PATH_MAX - 1] = '\0';
    rep.strlen_path = req->strlen_path;

    monta_caminho_fisico(fullpath, sizeof(fullpath), rootdir,
                         req->hdr.owner, req->path);

    // LOG: info da requisição
    printf("[SFSS WR] owner=%d path=\"%s\" full=\"%s\" offset=%ld -> ",
           req->hdr.owner, req->path, fullpath, (long)req->offset);
    fflush(stdout);

    // Abre arquivo para escrita (cria se não existir)
    fd = open(fullpath, O_WRONLY | O_CREAT, 0666);

    if (fd < 0) {
        // Erro ao abrir
        rep.offset = -1;
        perror("open");
        printf("\n");
    } 

    else {

        // Vai até o offset solicitado
        off = lseek(fd, req->offset, SEEK_SET);
        if (off < 0) {
            rep.offset = -2; // erro em lseek
            perror("lseek");
            printf("\n");
        } 

        else {

            // Escreve 16 bytes
            n = write(fd, req->payload, SFP_PAYLOAD_SIZE);

            if (n < 0) {
                rep.offset = -3; // erro em write
                perror("write");
                printf("\n");
            } 

            else {
                // Sucesso: devolve offset+16 como "novo EOF aproximado"
                rep.offset = req->offset + SFP_PAYLOAD_SIZE;
                printf("OK (novo offset ~ %ld)\n", (long)rep.offset);
            }
        }

        close(fd);
    }

    // Opcional: copiar payload escrito de volta
    memcpy(rep.payload, req->payload, SFP_PAYLOAD_SIZE);

    if (sendto(sockfd, &rep, sizeof(rep), 0,
               (struct sockaddr *) clientaddr, clientlen) < 0) {
        error("ERROR in sendto (WR-REP)");
    }
}

// Trata DC-REQ (criação de diretório: add)
static void trata_dc_req(int sockfd,
                         SFPDirCreateReq *req,
                         struct sockaddr_in *clientaddr,
                         socklen_t clientlen,
                         const char *rootdir) {
    SFPDirCreateRep rep;
    char parentPath[512];
    char fullpath[512];

    memset(&rep, 0, sizeof(rep));

    rep.hdr.type  = SFP_DC_REP;
    rep.hdr.owner = req->hdr.owner;

    // Monta caminho do diretório pai
    monta_caminho_fisico(parentPath, sizeof(parentPath), rootdir,
                         req->hdr.owner, req->path);

    // Caminho completo do novo diretório
    snprintf(fullpath, sizeof(fullpath), "%s/%s", parentPath, req->dirname);
    fullpath[sizeof(fullpath) - 1] = '\0';

    // LOG: info da requisição de diretório
    printf("[SFSS DC] owner=%d path=\"%s\" dirname=\"%s\" full=\"%s\" -> ",
           req->hdr.owner, req->path, req->dirname, fullpath);
    fflush(stdout);

    // Tenta criar diretório
    if (mkdir(fullpath, 0777) < 0) {
        rep.strlen_path = -1;
        rep.path[0] = '\0';
        perror("mkdir");
        printf("\n");
    } 

    else {

        printf("OK\n");

        // Monta o path lógico de volta: path + "/" + dirname
        if (req->strlen_path <= 0 || strcmp(req->path, ".") == 0) {
            // Se o path era vazio ou ".", o novo path lógico é só "dirname"
            strncpy(rep.path, req->dirname, SFP_PATH_MAX);
        } 
        
        else {
            snprintf(rep.path, SFP_PATH_MAX, "%s/%s", req->path, req->dirname);
        }

        rep.path[SFP_PATH_MAX - 1] = '\0';
        rep.strlen_path = (int) strlen(rep.path);
    }

    if (sendto(sockfd, &rep, sizeof(rep), 0,
               (struct sockaddr *) clientaddr, clientlen) < 0) {
        error("ERROR in sendto (DC-REP)");
    }
}

// Trata DR-REQ (remoção de arquivo ou diretório vazio: rem)
static void trata_dr_req(int sockfd,
                         SFPDirRemoveReq *req,
                         struct sockaddr_in *clientaddr,
                         socklen_t clientlen,
                         const char *rootdir) {
    SFPDirRemoveRep rep;
    char parentPath[512];
    char fullpath[512];
    struct stat st;
    int ret = -1;

    memset(&rep, 0, sizeof(rep));

    rep.hdr.type  = SFP_DR_REP;
    rep.hdr.owner = req->hdr.owner;

    // Caminho do diretório pai
    monta_caminho_fisico(parentPath, sizeof(parentPath), rootdir,
                         req->hdr.owner, req->path);

    // Caminho completo do alvo (arquivo ou diretório)
    snprintf(fullpath, sizeof(fullpath), "%s/%s", parentPath, req->name);
    fullpath[sizeof(fullpath) - 1] = '\0';

    // Decide se é arquivo ou diretório pelo stat
    if (stat(fullpath, &st) < 0) {
        ret = -1;
    } 
    
    else {
        
        if (S_ISDIR(st.st_mode)) {
            ret = rmdir(fullpath);  // remove diretório (tem que estar vazio)
        } 
        
        else {
            ret = unlink(fullpath); // remove arquivo
        }
    }

    if (ret < 0) {
        rep.strlen_path = -1;
        rep.path[0] = '\0';
    } 
    
    else {
        
        // Sucesso: path lógico sem o sufixo removido (basicamente, req->path)
        if (req->strlen_path <= 0 || strcmp(req->path, ".") == 0) {
            // Se era ".", volta vazio
            rep.path[0] = '\0';
            rep.strlen_path = 0;
        } 
        
        else {
            strncpy(rep.path, req->path, SFP_PATH_MAX);
            rep.path[SFP_PATH_MAX - 1] = '\0';
            rep.strlen_path = (int) strlen(rep.path);
        }
    }

    if (sendto(sockfd, &rep, sizeof(rep), 0, (struct sockaddr *) clientaddr, clientlen) < 0) {
        error("ERROR in sendto (DR-REP)");
    }
}

// Trata DL-REQ (listagem de diretório: listdir)
static void trata_dl_req(int sockfd,
                         SFPDirListReq *req,
                         struct sockaddr_in *clientaddr,
                         socklen_t clientlen,
                         const char *rootdir) {
    SFPDirListRep rep;
    char dirpath[512];
    DIR *dir;
    struct dirent *entry;
    int idx = 0;
    int pos = 0;

    memset(&rep, 0, sizeof(rep));

    rep.hdr.type  = SFP_DL_REP;
    rep.hdr.owner = req->hdr.owner;

    strncpy(rep.path, req->path, SFP_PATH_MAX);
    rep.path[SFP_PATH_MAX - 1] = '\0';
    rep.strlen_path = req->strlen_path;

    // Caminho físico do diretório
    monta_caminho_fisico(dirpath, sizeof(dirpath), rootdir,
                         req->hdr.owner, req->path);

    dir = opendir(dirpath);

    if (!dir) {
        rep.nrnames = -1; // erro ao abrir diretório
    } 
    
    else {
        
        while ((entry = readdir(dir)) != NULL) {
            const char *name = entry->d_name;

            // Ignora "." e ".."
            if (strcmp(name, ".") == 0 || strcmp(name, "..") == 0) {
                continue;
            }

            if (idx >= SFP_MAX_DIR_ENTRIES) {
                // Já atingimos o máximo de entradas que podemos armazenar
                break;
            }

            int nameLen = (int) strlen(name);
            
            if (pos + nameLen + 1 >= SFP_ALLFILENAMES_MAX) {
                // Não cabe mais nomes no buffer allfilenames
                break;
            }

            // Guarda posição inicial do nome
            int first = pos;

            // Copia nome para allfilenames
            memcpy(&rep.allfilenames[pos], name, nameLen);
            pos += nameLen;

            // Coloca um '\0' para separar os nomes
            rep.allfilenames[pos] = '\0';
            pos++;

            // Preenche a posição correspondente
            rep.positions[idx].first  = first;
            rep.positions[idx].last   = first + nameLen - 1;

            // Descobrindo se é diretório ou arquivo
            char full[512];
            struct stat st;

            snprintf(full, sizeof(full), "%s/%s", dirpath, name);
            full[sizeof(full) - 1] = '\0';

            if (stat(full, &st) == 0 && S_ISDIR(st.st_mode)) {
                rep.positions[idx].is_dir = 1;
            } 
            
            else {
                rep.positions[idx].is_dir = 0;
            }

            idx++;
        }

        closedir(dir);
        rep.nrnames = idx; // >= 0 se OK
    }

    if (sendto(sockfd, &rep, sizeof(rep), 0, (struct sockaddr *) clientaddr, clientlen) < 0) {
        error("ERROR in sendto (DL-REP)");
    }
}

// Entrypoint
int main(int argc, char **argv) {
    int sockfd;                     // socket
    int portno;                     // numero da prota
    socklen_t clientlen;            // tamanho em bytes do endereço do cliente
    struct sockaddr_in serveraddr;  // endereço do servidor
    struct sockaddr_in clientaddr;  // endereço do cliente
    int optval;                     // valor da flag para setsockopt
    ssize_t n;

    char rootdir[512];

    SFPMessage msg; // buffer de mensagem SFP

    if (argc < 3) {
        fprintf(stderr, "usage: %s <port> <rootdir>\n", argv[0]);
        exit(1);
    }

    portno = atoi(argv[1]);
    if (portno <= 0) {
        fprintf(stderr, "Porta inválida: %s\n", argv[1]);
        exit(1);
    }

    strncpy(rootdir, argv[2], sizeof(rootdir));
    rootdir[sizeof(rootdir) - 1] = '\0';

    // Cria socket UDP
    sockfd = socket(AF_INET, SOCK_DGRAM, 0);
    if (sockfd < 0) {
        error("ERROR opening socket");
    }

    // Permite reuso imediato da porta
    optval = 1;
    if (setsockopt(sockfd, SOL_SOCKET, SO_REUSEADDR, (const void *)&optval, sizeof(int)) < 0) {
        error("ERROR on setsockopt");
    }

    // Preenche endereço do servidor
    bzero((char *) &serveraddr, sizeof(serveraddr));

    serveraddr.sin_family = AF_INET;
    serveraddr.sin_addr.s_addr = htonl(INADDR_ANY);
    serveraddr.sin_port = htons((unsigned short) portno);

    // Faz bind
    if (bind(sockfd, (struct sockaddr *) &serveraddr, sizeof(serveraddr)) < 0) {
        error("ERROR on binding");
    }

    printf("SFSS rodando na porta %d, rootdir=\"%s\".\n", portno, rootdir);

    // Loop principal
    clientlen = sizeof(clientaddr);
    while (1) {
        // 4.1: recvfrom bloqueante aguardando mensagem do kernelSim
        n = recvfrom(sockfd, &msg, sizeof(msg), 0, (struct sockaddr *) &clientaddr, &clientlen);
        
        if (n < 0) {
            error("ERROR in recvfrom");
        }

        // 4.2: interpreta o cabeçalho (hdr.type, hdr.owner)
        int type  = msg.hdr.type;
        int owner = msg.hdr.owner;

        
        const char *tname = "UNKNOWN";
        switch (type) {
            case SFP_RD_REQ: tname = "RD-REQ (FILE READ)"; break;
            case SFP_WR_REQ: tname = "WR-REQ (FILE WRITE)"; break;
            case SFP_DC_REQ: tname = "DC-REQ (DIR ADD)"; break;
            case SFP_DR_REQ: tname = "DR-REQ (DIR REM)"; break;
            case SFP_DL_REQ: tname = "DL-REQ (DIR LIST)"; break;
        }

        printf("SFSS recebeu %s de owner=%d (bytes=%zd)\n", tname, owner, n);

        switch (type) {

            case SFP_RD_REQ:
                trata_rd_req(sockfd, &msg.rd_req, &clientaddr, clientlen, rootdir);
                break;

            case SFP_WR_REQ:
                trata_wr_req(sockfd, &msg.wr_req, &clientaddr, clientlen, rootdir);
                break;

            case SFP_DC_REQ:
                trata_dc_req(sockfd, &msg.dc_req, &clientaddr, clientlen, rootdir);
                break;

            case SFP_DR_REQ:
                trata_dr_req(sockfd, &msg.dr_req, &clientaddr, clientlen, rootdir);
                break;

            case SFP_DL_REQ:
                trata_dl_req(sockfd, &msg.dl_req, &clientaddr, clientlen, rootdir);
                break;

            default:
                fprintf(stderr, "SFSS: tipo de mensagem desconhecido: %d\n", type);
                // Poderia responder com erro genérico, se você quiser
                break;
        }
    }

    // Nunca chega aqui normalmente
    close(sockfd);
    return 0;
}
