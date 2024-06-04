#include "nthread-impl.h"
#include "nthread.h"
#include <pss.h>

/* ==================== NOTAS AUXILIAR =======================

    1. Explicar que es un mensaje, como funcionan, y que hacen.
        Que hacen:
        - Es una estructura de datos que se envia de un thread a otro
        - Se envia con nSend
        - Se recibe con nReceive
        - Se responde con nReply
        - El mensaje puede ser cualquier cosa, un int, un struct, etc. Pero usaremos estados
        Que hacen:
        - Se usa para sincronizar threads
        - Se usa para compartir informacion entre threads
        - Se puede usar para compartir recursos entre threads
        Como funcionan:
        - El thread que envia el mensaje se suspende hasta que llegue la respuesta
        - El thread que recibe el mensaje se suspende hasta recibir un mensaje
        - El thread que responde el mensaje no se suspende

    2. Explicar un ejemplo de uso: La impresora compartida
        Obtener y Devolver
        - Mas simple, puesto que suspendemos el thread en lugar de tener 
            que setear tantas formalidades
        ImpresoraServer
            1ra parte, No ocupada
            - nReceive tiene un timeout, tal que dejamos de esperar
                si nadie nos envia un mensaje y con eso podemos activar el modo
                de bajo consumo
            - Luego usamos un nReceive sin timeout para esperar a que alguien
                nos envie un mensaje
            2da parte, Ocupada
            - nReceive sin timeout puesto que estamos esperando hasta que el
            usuario termine de utilizar la impresora
            - Luego recibimos un mensaje y ejecutamos según el mensaje
            - OBTENER: 
                + Si la impresora esta ocupada, ponemos el thread solicitante
                   en una cola, este se suspende puesto que espera que respondamos
                + Si esta libre, seteamos la impresora como ocupada y respondemos
                    al thread solicitante
            - DEVOLVER:
                + Respondemos inmediatamente para indicar que recibimos el mensaje
                + Si hay threads en la cola de espera, respondemos al primero 
                    para desbloquearlo
                + Si no hay threads en la cola de espera, seteamos la impresora
                    como libre

    3. Escribir código de abajo
        nSend
        - hacer nSelf()
        - Nos ponemos en la cola de espera de th (para que nos responda)
        - Ponemos el mensaje y nos suspendemos (con estado WAIT_REPLY)
        - schedule ejecuta el cambio de estado en el core, efectuando la suspension
        - obtenemos la respuesta y retornamos
        - notar que hay otro caso adicional, si el thread th esta en estado WAIT_SEND
            lo ponemos en READY 
        
        - recapitular codigo 

        nReceive
        - primero armar sin timeout, luego se lo añadiremos
        - hacer nSelf()
        - Debemos conseguir el sender_thread desde la cola sendQueue, por lo que debemos ver que no sea vacia
        - Suspender si está vacia, cuando no lo este sacar el thread de la cola
        - dejar referencia al sender_thread en pth y obtener mensaje
        
        nReply
        - Fijar mensaje en variable rc del thread th (return code)
        - poner en ready al thread th

    4. Añadir timeout
        nReceive
        - expandir condición de suspensión ( ...  &&(if 0 no esperar))
        - si > 0, programar timer con nuevo estado, y poner tiempo en desde ms a ns (x 1M)
        - parchar cuando el timeout se acaba y que no haya mensaje

        nSend
        - expandir condición de suspensión ( ...  ||(status == WAIT_SEND_TIMEOUT))
        - cancelar timeout si el thread esta en WAIT_SEND_TIMEOUT
*/

// ================== Version sin timeout ===================


int nSend(nThread th, void *msg){
    START_CRITICAL

    // si el thread th esta en nRecieve, se pone en ready
    if(th->status == WAIT_SEND){
        setReady(th);
    }


    // me
    nThread this_th = nSelf();
    // ponerme en la cola de threads que quiere mandar un mensaje
    nth_putBack(th->sendQueue, this_th);
    this_th->msg = msg; // guardar mensaje

    // esperar respuesta
    suspend(WAIT_REPLY);
    schedule();

    int rc = this_th->rc;
    // retornar rc
    END_CRITICAL
    return rc;
}

void *nReceive(nThread *pth, int timeout){
    START_CRITICAL
    nThread this_th = nSelf();

    // Si no hay mensajes en cola
    if(nth_emptyQueue(this_th->sendQueue)){
        // esperar a que llegue mensaje
        suspend(WAIT_SEND);
        schedule();
    }

    // tomar 1er thread de la cola y retornar su mensaje
    nThread sender_th = nth_getFront(this_th->sendQueue);
    *pth = sender_th;
    void *msg = sender_th->msg;

    END_CRITICAL
    return msg;
}

void nReply(nThread th, int rc){
    START_CRITICAL
    
    th->rc = rc;
    setReady(th);
    scheduler();

    END_CRITICAL
}

// ================== Version con timeout ===================

// usaremos la función
// nth_programTimer(tiempo_ns, arg_ignorado)
// para suspender el thread por tiempo_ns nanosegundos

// receive debe tener un timeout que lo suspenda mientras espera
void *nReceive(nThread *pth, int timeout){
    START_CRITICAL
    nThread this_th = nSelf();

    // Si no hay mensajes en cola
    if(nth_emptyQueue(this_th->sendQueue) && timeout_ms != 0){
        if(timeout_ms > 0){
            // programar timer
            suspend(WAIT_SEND_TIMEOUT)
            nth_programTimer(timeout_ms * 1000000, NULL);
        } else {
            // esperar a que llegue mensaje
            suspend(WAIT_SEND);
        }

        
        schedule();
    }

    // tomar 1er thread de la cola y retornar su mensaje
    nThread sender_th = nth_getFront(this_th->sendQueue);

    // si el thread estaba en timeout y se despierta, no hay mensaje
    // por lo que tenemos que retornar NULL
    *pth = sender_th ? sender_th : NULL;
    // *pth = [thread_que_envio_mensaje] si existe [thread_que_envio_mensaje] de no ser así [NULL];
    // (python): pth = sender_th if sender_th else None

    void *msg = sender_th->msg;

    END_CRITICAL
    return msg;
}

// tenemos que cancelar timeout del thread al que le enviamos mensaje si es que tiene
int nSend(nThread th, void *msg){
    START_CRITICAL

    // si el thread th esta en nRecieve, se pone en ready
    if(th->status == WAIT_SEND || th->status == WAIT_SEND_TIMEOUT){
        if(th->status == WAIT_SEND_TIMEOUT){
            // cancelar timer si tiene timer
            nth_cancelThread(th);
        }
        setReady(th);
    }
    
    nThread this_th = nSelf(); // me
    // ponerme en la cola de threads que quiere mandar un mensaje
    nth_putBack(th->sendQueue, this_th);
    this_th->msg = msg; // guardar mensaje

    // esperar respuesta
    suspend(WAIT_REPLY);
    schedule();

    int rc = this_th->rc;
    // retornar rc
    END_CRITICAL
    return rc;
}

// no hay cambios
void nReply(nThread th, int rc){
    START_CRITICAL
    
    th->rc = rc;
    setReady(th);
    scheduler();

    END_CRITICAL
}