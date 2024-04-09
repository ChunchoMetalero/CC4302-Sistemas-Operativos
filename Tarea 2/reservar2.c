#define _XOPEN_SOURCE 500

#include <pthread.h>
#include <stdio.h>
#include <stdlib.h>
#include <unistd.h>

#include "reservar.h"

#define NUM_ESTACIONAMIENTOS 10

// Defina aca las variables globales y funciones auxiliares que necesite

pthread_mutex_t mutex;
pthread_cond_t cond;

int estacionamientos[NUM_ESTACIONAMIENTOS] = {0}; // 0 indica disponible, 1 indica ocupado
int llegada = 0; // Variable para llevar el orden de llegada de los vehículos

void initReservar() {
    pthread_mutex_init(&mutex, NULL);
    pthread_cond_init(&cond, NULL);
}

void cleanReservar() {
    pthread_mutex_destroy(&mutex);
    pthread_cond_destroy(&cond);
}

int reservar(int k) {
    pthread_mutex_lock(&mutex);

    int i, j, consecutivos = 0;
    int primer_estacionamiento = -1;

    while (1) {
        // Buscar k estacionamientos contiguos disponibles
        for (i = 0; i < NUM_ESTACIONAMIENTOS; i++) {
            if (estacionamientos[i] == 0) {
                // Contar estacionamientos contiguos disponibles
                for (j = i; j < NUM_ESTACIONAMIENTOS && estacionamientos[j] == 0; j++) {
                    consecutivos++;
                    if (consecutivos == k) {
                        primer_estacionamiento = i;
                        break;
                    }
                }
                if (consecutivos == k) break; // Encontró k estacionamientos contiguos
                else consecutivos = 0; // Reinicia contador de estacionamientos contiguos
            }
        }

        // Si encontró k estacionamientos contiguos disponibles, reserva y retorna
        if (consecutivos == k) {
            for (i = primer_estacionamiento; i < primer_estacionamiento + k; i++) {
                estacionamientos[i] = 1; // Marcar estacionamientos como ocupados
            }
            pthread_mutex_unlock(&mutex);
            return primer_estacionamiento;
        } else {
            // Espera a que haya k estacionamientos contiguos disponibles
            pthread_cond_wait(&cond, &mutex);
        }
    }
}

void liberar(int e, int k) {
    pthread_mutex_lock(&mutex);

    int i;

    // Liberar los k estacionamientos ocupados por el vehículo
    for (i = e; i < e + k; i++) {
        estacionamientos[i] = 0; // Marcar estacionamientos como disponibles
    }

    // Despertar a un posible vehículo en espera
    pthread_cond_signal(&cond);

    pthread_mutex_unlock(&mutex);
}