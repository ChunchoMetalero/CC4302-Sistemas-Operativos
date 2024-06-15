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

char *seller = NULL; //direccion del string con el nombre del vendedor
char *buyer = NULL; //direccion en donde el comprador debe copiar su nombre

int *seller_state = NULL; //direccion de la variable que indica el estado del vendedor
int *seller_mutex = NULL; //direccion de la variable que indica si el vendedor esta ocupado o no

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

  spinUnlock(&mutex);

  spinLock(&lk);
  spinLock(&mutex);

  if(state == 1) {
    strcpy(comprador, buyer);
    min_price = 0;
    seller = NULL;
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

  buyer = comprador;
  strcpy(vendedor, seller);

  *seller_state = 1;
  spinUnlock(seller_mutex);
  spinUnlock(&mutex);
  return min_price;
}
