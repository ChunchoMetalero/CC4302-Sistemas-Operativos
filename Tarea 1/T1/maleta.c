// Plantilla para maleta.c

#define _XOPEN_SOURCE 500

#include <stdio.h>
#include <unistd.h>
#include <stdlib.h>
#include <sys/types.h>
#include <sys/time.h>
#include <sys/wait.h>
#include <pthread.h>

#include "maleta.h"
#define P 8

// Defina aca las estructuras y funciones adicionales que necesite
// ...

typedef struct {
  double *w;
  double *v;
  int *z;
  int n;
  double maxW;
  int k;
  double res;
  
} Args;

void *thread(void *p) {
  Args *args = (Args *)p;
  double *w = args->w;
  double *v = args->v;
  int *z = args->z;
  int n = args->n;
  double maxW = args->maxW;
  int k = args->k;

  args->res = llenarMaletaSec(w, v, z, n, maxW, k);
  return NULL;
}

double llenarMaletaPar(double w[], double v[], int z[], int n, double maxW, int k) {
  pthread_t pid[P];
  Args args[P];
  int best = 0;
  int intervalo = k/P;
  for (int h = 0; h < P; h++) {
    
    args[h].w = w;
    args[h].v = v;
    int *j = (int *)malloc(n * sizeof(int));
    args[h].z = j;
    args[h].n = n;
    args[h].maxW = maxW;
    args[h].k = intervalo;
    pthread_create(&pid[h], NULL, thread, &args[h]);
  }

  double res = -1;
  for (int h = 0; h < P; h++) {
    pthread_join(pid[h], NULL);
    printf("res: %f\n", args[h].res);
    
    if (res < args[h].res){
      res = args[h].res;
      best = h;
    }
  }
  for (int h = 0; h < n; h++) {
    z[h] = args[best].z[h];
  }
  for (int h = 0; h < P; h++) {
    free(args[h].z);
  }

  return res;
}

