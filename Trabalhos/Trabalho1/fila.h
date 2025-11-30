#ifndef FILA_H
#define FILA_H

#include <sys/types.h>
#include <stddef.h>

typedef struct node {
    pid_t value;
    struct node *next;
} Node;

typedef struct fila {
    Node *head;
    Node *tail;
    size_t size;
} Fila;

void init(Fila *fila);
int empty(Fila *fila);
void push(Fila *fila, pid_t value);
pid_t pop(Fila *fila);
void clear(Fila *fila);

#endif
