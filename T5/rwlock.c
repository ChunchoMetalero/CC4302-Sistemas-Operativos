#define _XOPEN_SOURCE 500

#include "nthread-impl.h"

#include "rwlock.h"

struct rwlock {
  NthQueue *queue_readers;
  NthQueue *queue_writters;
  int readers;
  int writters;

};

nRWLock *nMakeRWLock() {
  nRWLock *rwl = malloc(sizeof(nRWLock));
  rwl->queue_readers = nth_makeQueue();
  rwl->queue_writters = nth_makeQueue();
  rwl->readers = 0;
  rwl->writters = 0;
  return rwl;
}

void eliminate(nThread th) {
  NthQueue *queue = (NthQueue *)th->ptr;
  nth_delQueue(queue, th);
  th->ptr = NULL;
}


void nDestroyRWLock(nRWLock *rwl) {
  nth_destroyQueue(rwl->queue_readers);
  nth_destroyQueue(rwl->queue_writters);
  free(rwl);
}

int nEnterRead(nRWLock *rwl, int timeout) {
  START_CRITICAL
  if (rwl->writters == 0 && nth_emptyQueue(rwl->queue_writters)){
    rwl->readers++;
  }
  else{
    nThread this_th = nSelf();
    nth_putBack(rwl->queue_readers, this_th);
    suspend(WAIT_RWLOCK);
    schedule();
  }
  END_CRITICAL
  return 1;
}

int nEnterWrite(nRWLock *rwl, int timeout) {
  START_CRITICAL
  if ((rwl->writters == 0) && (rwl->readers == 0)){
    rwl->writters++;
  }
  else{
    nThread this_th = nSelf();
    nth_putBack(rwl->queue_writters, this_th);
    this_th->ptr = rwl->queue_writters;
    if(timeout > 0) {
      suspend(WAIT_RWLOCK_TIMEOUT);
      nth_programTimer(timeout*1000000LL,eliminate);
    }
    else {
      suspend(WAIT_RWLOCK);
    }
    schedule();
    if (this_th->ptr == NULL){
      END_CRITICAL
      return 0;
    }
    else{
      END_CRITICAL
      return 1;
    }
  }
  END_CRITICAL
  return 1;
}

void nExitRead(nRWLock *rwl) {
  START_CRITICAL

  rwl->readers--;

  if(rwl->readers == 0 && !nth_emptyQueue(rwl->queue_writters)){
    nThread th = nth_getFront(rwl->queue_writters);
    if(th->status == WAIT_RWLOCK_TIMEOUT){
      nth_cancelThread(th);
    }
    setReady(th);
    rwl->writters++;
  }

  END_CRITICAL
}

void nExitWrite(nRWLock *rwl) {
  START_CRITICAL

  rwl->writters--;

  if (!nth_emptyQueue(rwl->queue_readers)){
    for (int i = 0; i <= nth_queueLength(rwl->queue_readers); i++){
      nThread th = nth_getFront(rwl->queue_readers);
      setReady(th);
      rwl->readers++;
    }
  }
  else if(nth_emptyQueue(rwl->queue_readers) && !nth_emptyQueue(rwl->queue_writters)){
    nThread th = nth_getFront(rwl->queue_writters);
    if(th->status == WAIT_RWLOCK_TIMEOUT){
      nth_cancelThread(th);
    }
    setReady(th);
    rwl->writters++;
  }

  END_CRITICAL
}