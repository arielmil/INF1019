#ifndef SFP_H
#define SFP_H

/* Protocolo SFP (Simple File Protocol)
 *
 * Todas as mensagens trocadas entre kernelSim e SFSS seguem
 * estes formatos de struct.
 *
 * Importante: kernelSim e SFSS rodam na mesma máquina,
 * então podemos usar structs C diretamente via sendto/recvfrom.
 * (Não estamos nos preocupando com endianess aqui.)
 */

/* Tamanhos máximos de campos de string usados nas mensagens */

#define SFP_PATH_MAX           256  /* Tamanho máximo do path relativo */
#define SFP_NAME_MAX            64  /* Tamanho máximo de nomes (arquivos/dirs) */
#define SFP_PAYLOAD_SIZE        16  /* Tamanho fixo dos blocos de arquivo */
#define SFP_ALLFILENAMES_MAX  1024  /* Buffer para todos os nomes em listdir */
#define SFP_MAX_DIR_ENTRIES     40  /* Máximo de entradas em um diretório listado */

/* Tipos de mensagem SFP
 *
 * Sempre que uma mensagem é enviada, o campo "type" deve
 * ser um destes valores.
 */

#define SFP_RD_REQ   1  /* Requisição de leitura de arquivo (read)  */
#define SFP_RD_REP   2  /* Resposta de leitura de arquivo           */
#define SFP_WR_REQ   3  /* Requisição de escrita de arquivo (write) */
#define SFP_WR_REP   4  /* Resposta de escrita de arquivo           */
#define SFP_DC_REQ   5  /* Requisição de criação de diretório (add) */
#define SFP_DC_REP   6  /* Resposta de criação de diretório         */
#define SFP_DR_REQ   7  /* Requisição de remoção (arquivo/dir) (rem)*/
#define SFP_DR_REP   8  /* Resposta de remoção                      */
#define SFP_DL_REQ   9  /* Requisição de listagem de diretório      */
#define SFP_DL_REP  10  /* Resposta de listagem de diretório        */

/* Cabeçalho comum a todas as mensagens
 *
 * type  -> um dos SFP_*_REQ ou SFP_*_REP
 * owner -> qual processo Ax fez a chamada (1..5)
 */

typedef struct sfp_header {
    int type;
    int owner;
} SFPHeader;

/* Estrutura auxiliar usada na resposta de listdir (DL-REP)
 *
 * first  -> índice inicial do nome em allfilenames
 * last   -> índice final  do nome em allfilenames (inclusive)
 * is_dir -> 1 se for subdiretório, 0 se for arquivo
 */

typedef struct sfp_name_pos {
    int first;
    int last;
    int is_dir;
} SFPNamePos;

/* RD-REQ: leitura de 16 bytes de arquivo a partir de offset
 *
 * Semântica:
 * - path         -> caminho relativo ao "home" do owner (ex: "dir1/arq.txt")
 * - strlen_path  -> strlen(path)
 * - payload      -> não usado (deixar zerado)
 * - offset       -> posição a ler (0,16,32,...) dentro do arquivo
 */

typedef struct sfp_rd_req {
    SFPHeader hdr;
    char path[SFP_PATH_MAX];
    int strlen_path;
    char payload[SFP_PAYLOAD_SIZE]; /* não usado em leitura */
    int offset;
} SFPReadReq;

/* RD-REP: resposta de leitura
 *
 * - path         -> mesmo path da requisição (opcional para debug)
 * - strlen_path  -> normalmente strlen(path)
 * - payload      -> bytes lidos (até 16 bytes); se erro, conteúdo irrelevante
 * - offset       -> offset pedido se sucesso;
 *                   valor < 0 indica erro (por exemplo -1)
 */

typedef struct sfp_rd_rep {
    SFPHeader hdr;
    char path[SFP_PATH_MAX];
    int strlen_path;
    char payload[SFP_PAYLOAD_SIZE];
    int offset; /* >= 0 se OK, < 0 se erro */
} SFPReadRep;

/* WR-REQ: escrita de 16 bytes de arquivo a partir de offset
 *
 * - path         -> caminho relativo ao "home" do owner
 * - strlen_path  -> strlen(path)
 * - payload      -> 16 bytes a serem escritos
 * - offset       -> posição onde escrever (0,16,32,...)
 */

typedef struct sfp_wr_req {
    SFPHeader hdr;
    char path[SFP_PATH_MAX];
    int strlen_path;
    char payload[SFP_PAYLOAD_SIZE];
    int offset;
} SFPWriteReq;

/* WR-REP: resposta de escrita
 *
 * - path         -> mesmo path da requisição
 * - strlen_path  -> strlen(path) ou < 0 indicando erro
 * - payload      -> opcional (pode devolver o que foi escrito ou zerado)
 * - offset       -> >= 0 se OK (pode ser o novo EOF ou o offset escrito);
 *                   < 0 se erro
 */

typedef struct sfp_wr_rep {
    SFPHeader hdr;
    char path[SFP_PATH_MAX];
    int strlen_path;
    char payload[SFP_PAYLOAD_SIZE];
    int offset; /* >= 0 se OK, < 0 se erro */
} SFPWriteRep;

/* DC-REQ: criação de diretório (add)
 *
 * - path            -> diretório pai (relativo ao "home" do owner)
 * - strlen_path     -> strlen(path)
 * - dirname         -> nome do novo subdiretório
 * - strlen_dirname  -> strlen(dirname)
 */

typedef struct sfp_dc_req {
    SFPHeader hdr;
    char path[SFP_PATH_MAX];
    int strlen_path;
    char dirname[SFP_NAME_MAX];
    int strlen_dirname;
} SFPDirCreateReq;

/* DC-REP: resposta de criação de diretório
 *
 * - path        -> path lógico já contendo o novo sufixo (path + "/dirname")
 * - strlen_path -> >= 0 se OK; < 0 se erro
 */

typedef struct sfp_dc_rep {
    SFPHeader hdr;
    char path[SFP_PATH_MAX];
    int strlen_path;
} SFPDirCreateRep;

/* DR-REQ: remoção de arquivo ou diretório vazio (rem)
 *
 * - path        -> diretório pai
 * - strlen_path -> strlen(path)
 * - name        -> nome do arquivo ou diretório a remover
 * - strlen_name -> strlen(name)
 */

typedef struct sfp_dr_req {
    SFPHeader hdr;
    char path[SFP_PATH_MAX];
    int strlen_path;
    char name[SFP_NAME_MAX];
    int strlen_name;
} SFPDirRemoveReq;

/* DR-REP: resposta de remoção
 *
 * - path        -> path lógico já sem o sufixo removido, se sucesso
 * - strlen_path -> >= 0 se OK; < 0 se erro
 */

typedef struct sfp_dr_rep {
    SFPHeader hdr;
    char path[SFP_PATH_MAX];
    int strlen_path;
} SFPDirRemoveRep;

/* DL-REQ: listagem de diretório (listdir)
 *
 * - path        -> diretório a listar (relativo ao "home" do owner)
 * - strlen_path -> strlen(path)
 */

typedef struct sfp_dl_req {
    SFPHeader hdr;
    char path[SFP_PATH_MAX];
    int strlen_path;
} SFPDirListReq;

/* DL-REP: resposta de listagem de diretório
 *
 * - path           -> diretório listado
 * - strlen_path    -> strlen(path); pode ser < 0 em erro geral
 * - allfilenames   -> todos os nomes concatenados em um único buffer
 * - positions[]    -> para cada nome:
 *                      * first/last: índices em allfilenames
 *                      * is_dir: 1 se diretório, 0 se arquivo
 * - nrnames        -> número de entradas válidas em positions;
 *                     < 0 indica erro
 */

typedef struct sfp_dl_rep {
    SFPHeader hdr;
    char path[SFP_PATH_MAX];
    int strlen_path;
    char allfilenames[SFP_ALLFILENAMES_MAX];
    SFPNamePos positions[SFP_MAX_DIR_ENTRIES];
    int nrnames; /* >= 0 se OK, < 0 se erro */
} SFPDirListRep;

/* União de mensagens SFP
 *
 * Útil para fazer um único recvfrom/sendto no kernelSim e no SFSS.
 * Você pode declarar:
 *
 *   SFPMessage msg;
 *   recvfrom(sock, &msg, sizeof(msg), ...);
 *
 * e depois checar msg.hdr.type para saber qual campo usar.
 */

typedef union sfp_message {
    SFPHeader       hdr;
    SFPReadReq      rd_req;
    SFPReadRep      rd_rep;
    SFPWriteReq     wr_req;
    SFPWriteRep     wr_rep;
    SFPDirCreateReq dc_req;
    SFPDirCreateRep dc_rep;
    SFPDirRemoveReq dr_req;
    SFPDirRemoveRep dr_rep;
    SFPDirListReq   dl_req;
    SFPDirListRep   dl_rep;
} SFPMessage;

#endif /* SFP_H */
