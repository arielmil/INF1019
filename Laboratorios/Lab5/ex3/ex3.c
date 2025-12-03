#include <stdio.h>
#include <stdlib.h>
#include <pthread.h>
#include <unistd.h>  // para sleep, random e srand

// Tamanho do buffer e quantidade total de itens que serão produzidos
#define BUFFER_SIZE 8
#define NUM_ITEMS   64

// Buffer circular compartilhado
int buffer[BUFFER_SIZE];
int count = 0;       // quantos itens existem no buffer
int in = 0;          // índice de escrita (produtor)
int out = 0;         // índice de leitura (consumidor)

// Mutex que protege o acesso ao buffer e às variáveis acima
pthread_mutex_t mutex = PTHREAD_MUTEX_INITIALIZER;

// Variáveis de condição:
// can_produce: produtor espera aqui quando o buffer está cheio
// can_consume: consumidor espera aqui quando o buffer está vazio
pthread_cond_t can_produce = PTHREAD_COND_INITIALIZER;
pthread_cond_t can_consume = PTHREAD_COND_INITIALIZER;

void *produtor(void *arg) {
    int item;

    for (int i = 0; i < NUM_ITEMS; i++) {
        // Entra na região crítica
        pthread_mutex_lock(&mutex);

        // Enquanto buffer estiver cheio, produtor espera
        while (count == BUFFER_SIZE) {
            printf("[produtor] Buffer cheio. Esperando...\n");

            pthread_cond_wait(&can_produce, &mutex);
        }

        item = rand() + i;
        buffer[in] = item;                    // produz item i

        printf("[produtor] Produzi %d na posição %d\n", item, in);

        in = (in + 1) % BUFFER_SIZE;      // avança índice circular
        count++;                          // mais um item no buffer

        // Sinaliza que agora existe pelo menos um item para consumir
        pthread_cond_signal(&can_consume);

        // Sai da região crítica
        pthread_mutex_unlock(&mutex);

        // Dorme por 1 seg
        sleep(1);
    }

    pthread_exit(NULL);
}

void *consumidor(void *arg) {
    for (int i = 0; i < NUM_ITEMS; i++) {
        // Entra na região crítica
        pthread_mutex_lock(&mutex);

        // Enquanto buffer estiver vazio, consumidor espera
        while (count == 0) {
            printf("[CONSUMIDOR] Buffer vazio. Vou esperar...\n");
            
            pthread_cond_wait(&can_consume, &mutex);
        }

        // Aqui há pelo menos um item: podemos consumir
        int item = buffer[out];
        printf("[CONSUMIDOR] Consumi %d da posição %d\n", item, out);
        out = (out + 1) % BUFFER_SIZE;    // avança índice circular
        count--;                          // menos um item no buffer

        // Sinaliza que agora existe espaço para produzir
        pthread_cond_signal(&can_produce);

        // Sai da região crítica
        pthread_mutex_unlock(&mutex);

        // Dorme por 2 segs
        sleep(2);
    }

    pthread_exit(NULL);
}

int main(void) {
    pthread_t prod_thread, cons_thread;

    srand((unsigned)(getpid() ^ time(NULL))); // Para seedar a função rand()

    // Cria as threads produtor e consumidor
    if (pthread_create(&prod_thread, NULL, produtor, NULL) != 0) {
        perror("Erro ao criar thread produtora");
        exit(1);
    }

    if (pthread_create(&cons_thread, NULL, consumidor, NULL) != 0) {
        perror("Erro ao criar thread consumidora");
        exit(1);
    }

    // Espera as duas threads terminarem
    pthread_join(prod_thread, NULL);
    pthread_join(cons_thread, NULL);

    printf("Fim: todos os %d itens foram produzidos e consumidos.\n", NUM_ITEMS);

    // Destroi mutex e variáveis de condição
    pthread_mutex_destroy(&mutex);
    pthread_cond_destroy(&can_produce);
    pthread_cond_destroy(&can_consume);

    return 0;
}
