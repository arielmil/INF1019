#include <sys/types.h> // Para tipos como pid_t
#include <stdlib.h>    // Para malloc e free
#include "fila.h"

void init(Fila *fila) {
    fila->head = NULL;
    fila->tail = NULL;
    fila->size = 0;
}

int empty(Fila *fila) {
    if (fila->size == 0) {
        return 1;
    } 
    
    else {
        return 0;
    }
}

void push(Fila *fila, pid_t value) {
    Node *node = (Node *) malloc(sizeof(Node));
    node->value = value;
    node->next = NULL;

    if (fila->tail == NULL) {
        fila->head = node;
        fila->tail = node;
    } 
    
    else {
        fila->tail->next = node;
        fila->tail = node;
    }

    fila->size = fila->size + 1;
}

pid_t pop(Fila *fila) {
    if (fila->head == NULL) {
        return -1;
    } 
    
    else {
        Node *node = fila->head;
        pid_t v = node->value;
        fila->head = node->next;
        
        if (fila->head == NULL) {
            fila->tail = NULL;
        }

        fila->size = fila->size - 1;
        free(node);
        return v;
    }
}

void clear(Fila *fila) {
    while (fila->head != NULL) {
        Node *node = fila->head;
        fila->head = node->next;
        free(node);
    }
    fila->tail = NULL;
    fila->size = 0;
}