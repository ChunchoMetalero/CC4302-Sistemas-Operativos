#include <stdlib.h>
#include <stdio.h>
#include <unistd.h>
#include <string.h>

#include "pss.h"
#include "bolsa.h"
#include "spinlocks.h"

// Declare aca sus variables globales

int mutex = OPEN;
int min_price = 0;

char *seller = NULL; 
char *buyer = NULL; 

int *seller_state = NULL; 
int *seller_mutex = NULL; 

int vendo(int precio, char *vendedor, char *comprador) {
  spinLock(&mutex);

  int state = 0;
  int lk = CLOSED;

  if(precio > min_price && min_price != 0){
    spinUnlock(&mutex);
    return 0;
  }

  if(seller != NULL){
    seller_state = 0;
    spinUnlock(seller_mutex);
  }

  seller_mutex = &lk;
  seller_state = &state;

  min_price = precio;
  seller = vendedor;
  buyer = comprador;

  spinUnlock(&mutex);

  spinLock(&lk); // Espera a que el comprador compre
  spinLock(&mutex);

  if(state == 1) {
    spinUnlock(&mutex);
    return 1;
  }

  else{
    spinUnlock(&mutex);
    return 0;
  }
}

int compro(char *comprador, char *vendedor) {
  spinLock(&mutex);


  if(min_price == 0){
    spinUnlock(&mutex);
    return 0;
  }

  strcpy(vendedor, seller);
  strcpy(buyer, comprador);
  
  min_price = 0;
  seller = NULL;
  *seller_state = 1;
  
  spinUnlock(seller_mutex);
  spinUnlock(&mutex);
  return min_price;
}
