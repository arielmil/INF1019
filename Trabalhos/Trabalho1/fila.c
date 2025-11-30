#include <sys/types.h> // Para tipos como pid_t
#include <stdio.h>
#include <stdlib.h>

typedef struct node {
    pid_t value;
    struct node *next;
} Node;

typedef struct fila { 
    Node *head; 
    Node *tail; 
    size_t size; 
} Fila;

void init(Fila *fila) {
    fila->size = 0;
    fila->head = NULL;
    fila->tail = fila->head;
}

int empty(Fila *fila) {
    return (fila->size) == 0;
}

void push(Fila *fila, pid_t pid) {
    Node *node = (Node *)malloc(sizeof(Node));
    if (node == NULL) { 
        perror("Erro em usar malloc para um node. Saindo..."); 
        exit(-15);
    }

    node->value = pid;
    node->next = NULL;

    if (fila->tail) {
        fila->tail->next = node;
        fila->tail = node;
    } 
    
    else {
        // fila vazia
        fila->head = fila->tail = node;
    }


    fila->size++;
}


pid_t pop(Fila *fila) {
    if (fila->size == 0) {
        perror("Erro. Fila está vazia, mas alguém tentou usar pop(). Saindo...");
        exit(-16);
    }
    
    Node *n = fila->head;
    pid_t v = n->value;

    fila->head = n->next;          // avança o head
    if (fila->head == NULL)        // esvaziou
        fila->tail = NULL;

    fila->size--;
    free(n);
    return v;
}

void clear(Fila *fila) {
    while(fila->head) {
        Node *node = fila->head;
        fila->head = node->next;
        free(node);
    }

    fila->size = 0;
}