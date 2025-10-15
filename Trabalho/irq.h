#ifndef IRQ_H
#define IRQ_H

#include <signal.h>

// Mapa fixo de sinais para IRQs (simples e portátil)
// CORREÇÃO: usar apenas sinais não conflituosos com Ctrl-C (SIGINT) e término
// Usaremos SIGUSR1 (IRQ0), SIGUSR2 (IRQ1) e SIGHUP (IRQ2)
#define SIG_IRQ0 SIGUSR1   // Fim do quantum (preempção RR)
#define SIG_IRQ1 SIGUSR2   // Dispositivo D1 liberou 1 processo
#define SIG_IRQ2 SIGHUP    // Dispositivo D2 liberou 1 processo

#endif