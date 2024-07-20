#include <stdlib.h>
#include <unistd.h>
#include <string.h>
#include <stdio.h>
#include <pthread.h>

#include "disk.h"
#include "pss.h"

/*****************************************************
 * Agregue aca los tipos, variables globales u otras
 * funciones que necesite
 *****************************************************/

typedef struct {
    int ready;
    int track;
    pthread_cond_t w;
} Request;

pthread_mutex_t m = PTHREAD_MUTEX_INITIALIZER;

int busy = 0;
int c_track = 0;

PriQueue *p;
PriQueue *q;

void swapQueue(PriQueue *a, PriQueue *b){
    while (!emptyPriQueue(a)){
        Request *req = priGet(a);
        priPut(b, req, req->track);
    }
}


void iniDisk(void) {
    p = makePriQueue();
    q = makePriQueue();
}

void cleanDisk(void) {
    destroyPriQueue(p);
    destroyPriQueue(q);
}

void requestDisk(int track) {
    pthread_mutex_lock(&m);
    if(!busy){
        busy = 1;
        c_track = track;
    }
    else{
        Request req = {0, track, PTHREAD_COND_INITIALIZER};
        if (track < c_track){
            priPut(p, &req, track);
        }
        else{
            priPut(q, &req, track);
        }
        while(!req.ready){
            pthread_cond_wait(&req.w, &m);
            c_track = track;
        }

    }
    pthread_mutex_unlock(&m);
}

void releaseDisk() {
    pthread_mutex_lock(&m);
    if(emptyPriQueue(q) && emptyPriQueue(p)) {
        busy = 0;
    }
    else if(emptyPriQueue(q)){
        Request *req = priGet(p);
        req->ready = 1;
        swapQueue(p, q);
        pthread_cond_signal(&req->w);
    }
    else{
        Request *req = priGet(q);
        req->ready = 1;
        pthread_cond_signal(&req->w);
    }
    pthread_mutex_unlock(&m);
}

