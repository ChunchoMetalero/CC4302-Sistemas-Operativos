#define _XOPEN_SOURCE 500

#include <pthread.h>
#include <stdio.h>
#include <stdlib.h>
#include <unistd.h>

#include "reservar.h"

// Defina aca las variables globales y funciones auxiliares que necesite

#define NUM_ESTACIONAMIENTOS 10

typedef struct {
  int consecutivos;
  int primer_estacionamiento;
} Contiguos;


pthread_mutex_t m = PTHREAD_MUTEX_INITIALIZER;
pthread_cond_t c = PTHREAD_COND_INITIALIZER;

int estacionamientos[NUM_ESTACIONAMIENTOS] = {0}; // 0 indica disponible, 1 indica ocupado

int ticket_dist = 0, display = 0;
int readers = 0;


Contiguos cantidadContiguosDisponibles(int k){
  int i = 0;
  int j = 0;
  Contiguos contiguos;
  contiguos.consecutivos = 0;
  contiguos.primer_estacionamiento = -1;

  for (i = 0; i < NUM_ESTACIONAMIENTOS; i++) {
    if (estacionamientos[i] == 0) {
      for(j = i; j < NUM_ESTACIONAMIENTOS && estacionamientos[j] == 0; j++){
        contiguos.consecutivos++;
        if (contiguos.consecutivos == k){
          contiguos.primer_estacionamiento = i;
          break;
        }
      }
    } 
    if (contiguos.consecutivos == k) break;
    else contiguos.consecutivos = 0;
  }
  return contiguos;
}

void initReservar() {
}

void cleanReservar() {
}

int reservar(int k) {
  pthread_mutex_lock(&m);
  int my_num = ticket_dist++;

  Contiguos contiguos = cantidadContiguosDisponibles(k);
  while (my_num > display || contiguos.consecutivos < k) {
    pthread_cond_wait(&c, &m);
    contiguos = cantidadContiguosDisponibles(k);
  }
  for (int i = contiguos.primer_estacionamiento; i < contiguos.primer_estacionamiento + k; i++) {
    estacionamientos[i] = 1;
  }
  display++;
  pthread_cond_broadcast(&c);
  pthread_mutex_unlock(&m);
  return contiguos.primer_estacionamiento;
}

void liberar(int e, int k) {
  pthread_mutex_lock(&m);
  for (int i = e; i < e + k; i++) {
    estacionamientos[i] = 0;
  }
  pthread_cond_broadcast(&c);
  pthread_mutex_unlock(&m);
} 
