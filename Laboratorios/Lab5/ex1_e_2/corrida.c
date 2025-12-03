#include <pthread.h> 
#include <stdio.h>
#include <stdlib.h>
#include <unistd.h>

#define NUM_THREADS 5 
#define PULO_MAXIMO 100 
#define DESCANSO_MAXIMO 1 
#define DISTANCIA_PARA_CORRER 100

static int classificacao = 1; 
static pthread_mutex_t lock = PTHREAD_MUTEX_INITIALIZER;
static char *resp[200]; 
 
static int cont = 0;

void *Correr(void *sapo)   { 
    int pulos = 0; 
    int distanciaJaCorrida = 0;

    while (distanciaJaCorrida <= DISTANCIA_PARA_CORRER) {
        int pulo = rand() % PULO_MAXIMO;
        distanciaJaCorrida += pulo;
        pulos++;
        printf("Sapo %d pulou\n", (int) sapo);

        int descanso = rand() % DESCANSO_MAXIMO;
        printf("Sapo %d vai dormir por %d segundos.\n", (int) sapo, descanso);
        sleep(descanso);
    } 
    
    pthread_mutex_lock(&lock);

    printf("\nSapo %d chegou na posicaoo %d com %d pulos\n\n", (int) sapo, classificacao, pulos); 
    
    cont++; 
    classificacao++;

    pthread_mutex_unlock(&lock);

    pthread_exit(NULL); 
}


int main()   { 
    classificacao = 1; 
    pthread_t threads[NUM_THREADS]; 
    int t; 
    printf("Corrida iniciada ... \n"); 
 
    for(t=0;t < NUM_THREADS;t++)    {
        pthread_create(&threads[t], NULL, Correr, (void *) t);
    } 
    
    for(t=0;t < NUM_THREADS; t++)   {
        pthread_join(threads[t],NULL);
    }
 
    printf("\nAcabou!!\n"); 
    pthread_exit(NULL); 
}