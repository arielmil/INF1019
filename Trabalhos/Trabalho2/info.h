#ifndef INFO_H
#define INFO_H

#include <sys/types.h> // Para tipos como pid_t

// Limites gerais

#define MAX 10   // Máximo de "instruções" (PC < MAX) por processo

// Estados possíveis de um processo

// READY        -> pronto para rodar
// WAITING_FILE -> bloqueado esperando resposta de operação de ARQUIVO (read/write)
// WAITING_DIR  -> bloqueado esperando resposta de operação de DIRETÓRIO (add/rem/listdir)
// RUNNING      -> atualmente em execução
// STOPPED      -> parado por IRQ0 (fatia de tempo acabou)
// TERMINATED   -> terminou (PC >= MAX ou finalização)
//

#define READY         0
#define WAITING_FILE  1
#define WAITING_DIR   2
#define RUNNING       3
#define STOPPED       4
#define TERMINATED    5

// Convenções para operações de sistema de arquivos

// fs_kind: indica se a syscall é de ARQUIVO ou DIRETÓRIO
//   'F' -> operação de arquivo (read/write)
//   'D' -> operação de diretório (add/rem/listdir)
//
// fs_op: indica qual operação específica foi pedida
//   'R' -> read
//   'W' -> write
//   'A' -> add      (criação de subdiretório)
//   'M' -> rem      (remoção de arquivo ou diretório)
//   'L' -> listdir  (listagem de diretório)
//

#define FS_KIND_FILE  'F'
#define FS_KIND_DIR   'D'

#define FS_OP_READ    'R'
#define FS_OP_WRITE   'W'
#define FS_OP_ADD     'A'
#define FS_OP_REM     'M'
#define FS_OP_LIST    'L'

// Tamanhos máximos usados para parâmetros de FS.
// (Devem bater com o que usamos em process.c e kernelSim.c)

#define FS_MAX_PATH       256
#define FS_MAX_NAME        64
#define FS_PAYLOAD_SIZE    16

// Estrutura Info - uma por processo A1..A5

// Campos antigos (Trabalho 1), com semântica adaptada:
//
//  state           -> estado atual do processo (READY, WAITING_FILE, etc.)
//  lastD           -> último "tipo" de recurso acessado:
//                      'F' para operação de arquivo
//                      'D' para operação de diretório
//                      '-' se ainda não acessou nada
//  lastOp          -> última operação feita nesse recurso:
//                      'R', 'W', 'A', 'M', 'L' ou '-' se nenhuma
//  PC              -> contador de programa (0..MAX-1)
//  timesD1Acessed  -> agora: quantas vezes fez operação de ARQUIVO
//  timesD2Acessed  -> agora: quantas vezes fez operação de DIRETÓRIO
//  pid             -> pid real do processo Ax
//
// Campos novos (Trabalho 2), usados para passar parâmetros de FS
// entre o processo Ax e o kernelSim:
//
//  fs_kind         -> 'F' ou 'D' (arquivo ou diretório)
//  fs_op           -> 'R','W','A','M','L'
//  fs_path         -> caminho relativo dentro do "home" do processo
//                     (ex.: "dir1/file2", "dir3/sub1")
//  fs_offset       -> usado para read/write; múltiplos de 16
//  fs_payload      -> 16 bytes usados em write (conteúdo a escrever)
//                     (pode ser reutilizado pelo kernelSim como buffer
//                      para dados lidos em um read, se quiser)
//  fs_name         -> nome de subdiretório ou arquivo (para add/rem)
//                     (não é usado para listdir)
//

typedef struct info {
    char state;       // 0: READY, 1: WAITING_FILE, 2: WAITING_DIR, 3: RUNNING, 4: STOPPED, 5: TERMINATED
    char lastD;       // 'F' para arquivo, 'D' para diretório, '-' se nenhum
    char lastOp;      // 'R','W','A','M','L' ou '-' se nenhuma
    int  PC;          // Contador de Programa
    int  timesD1Acessed; // agora: quantas operações de ARQUIVO este processo fez
    int  timesD2Acessed; // agora: quantas operações de DIRETÓRIO este processo fez
    pid_t pid;        // pid do processo

    // ---- Novos campos para syscalls de sistema de arquivos ----
    char fs_kind;                         // 'F' (arquivo) ou 'D' (diretório)
    char fs_op;                           // 'R','W','A','M','L'
    char fs_path[FS_MAX_PATH];            // path relativo (ex.: "dir1/file3")
    int  fs_offset;                       // offset usado em read/write (0,16,32,...)
    char fs_payload[FS_PAYLOAD_SIZE];     // 16 bytes de dados para write (ou buffer de read)
    char fs_name[FS_MAX_NAME];            // nome de arquivo/subdiretório (add/rem)
} Info;

#endif
