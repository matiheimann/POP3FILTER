#include <stdio.h>
#include <stdlib.h>  // malloc
#include <string.h>  // memset
#include <assert.h>  // assert
#include <errno.h>
#include <unistd.h>  // close
#include <arpa/inet.h>
#include <netdb.h>

#include "pop3filter.h"
#include "selector.h"
#include "stm.h"
#include "buffer.h"

#define N(x) (sizeof(x)/sizeof((x)[0]))

/** maquina de estados general */
// TODO enum pop3filter_state {};

////////////////////////////////////////////////////////////////////
// Definición de variables para cada estado

/*
 * Si bien cada estado tiene su propio struct que le da un alcance
 * acotado, disponemos de la siguiente estructura para hacer una única
 * alocación cuando recibimos la conexión.
 *
 * Se utiliza un contador de referencias (references) para saber cuando debemos
 * liberarlo finalmente, y un pool para reusar alocaciones previas.
 */
struct pop3filter {
    /** información del cliente */
    struct sockaddr_storage       client_addr;
    socklen_t                     client_addr_len;
    int                           client_fd;

    /** resolución de la dirección del origin server */
    struct addrinfo              *origin_resolution;
    /** intento actual de la dirección del origin server */
    struct addrinfo              *origin_resolution_current;

    /** información del origin server */
    struct sockaddr_storage       origin_addr;
    socklen_t                     origin_addr_len;
    int                           origin_domain;
    int                           origin_fd;


    /** maquinas de estados */
    struct state_machine          stm;

    /** estados para el client_fd */
    /* TODO
    union {
        
    } client;
    */
    /** estados para el origin_fd */
    /* TODO
    union {
        
    } orig;
    */
    /** buffers para ser usados read_buffer, write_buffer.*/
    uint8_t raw_buff_a[2048], raw_buff_b[2048]; // TODO definir tamaño
    buffer read_buffer, write_buffer;
    
    /** cantidad de referencias a este objeto. si es uno se debe destruir */
    unsigned references;

    /** siguiente en el pool */
    struct pop3filter *next;
};

static const unsigned  max_pool  = 50; // TODO tamaño máximo
static unsigned        pool_size = 0;  // tamaño actual
static struct pop3filter * pool  = 0;  // pool propiamente dicho

/** crea un nuevo `struct socks5' */
static struct pop3filter *
pop3filter_new(int client_fd) {
    struct pop3filter *ret;

    if(pool == NULL) {
        ret = malloc(sizeof(*ret));
    } else {
        ret       = pool;
        pool      = pool->next;
        ret->next = 0;
    }
    if(ret == NULL) {
        goto finally;
    }
    memset(ret, 0x00, sizeof(*ret));

    ret->origin_fd       = -1;
    ret->client_fd       = client_fd;
    ret->client_addr_len = sizeof(ret->client_addr);

    //ret->stm    .initial   = HELLO_READ; TODO
    //ret->stm    .max_state = ERROR; TODO
    //ret->stm    .states    = socks5_describe_states(); TODO
    stm_init(&ret->stm);

    buffer_init(&ret->read_buffer,  N(ret->raw_buff_a), ret->raw_buff_a);
    buffer_init(&ret->write_buffer, N(ret->raw_buff_b), ret->raw_buff_b);

    ret->references = 1;
finally:
    return ret;
}

/** obtiene el struct (socks5 *) desde la llave de selección  */
#define ATTACHMENT(key) ( (struct pop3filter *)(key)->data)

/** realmente destruye */
static void
pop3filter_destroy_(struct pop3filter* pop3) {
    if(pop3->origin_resolution != NULL) {
        freeaddrinfo(pop3->origin_resolution);
        pop3->origin_resolution = 0;
    }
    free(pop3);
}

/*
 * destruye un  `struct pop3filter', tiene en cuenta las referencias
 * y el pool de objetos.
 */
static void
pop3filter_destroy(struct pop3filter *pop3) {
    if(pop3 == NULL) {
        // nada para hacer
    } else if(pop3->references == 1) {
        if(pop3 != NULL) {
            if(pool_size < max_pool) {
                pop3->next = pool;
                pool    = pop3;
                pool_size++;
            } else {
                pop3filter_destroy_(pop3);
            }
        }
    } else {
        pop3->references -= 1;
    }
}

void
pop3filter_pool_destroy(void) {
    struct pop3filter *next, *pop3;
    for(pop3 = pool; pop3 != NULL ; pop3 = next) {
        next = pop3->next;
        free(pop3);
    }
}

/* declaración forward de los handlers de selección de una conexión
 * establecida entre un cliente y el proxy.
 */
/* TODO
static void pop3filter_read   (struct selector_key *key);
static void pop3filter_write  (struct selector_key *key);
static void pop3filter_block  (struct selector_key *key);
static void pop3filter_close  (struct selector_key *key);
static const struct fd_handler pop3filter_handler = {
    .handle_read   = pop3filter_read,
    .handle_write  = pop3filter_write,
    .handle_close  = pop3filter_close,
    .handle_block  = pop3filter_block,
};*/ 

/* Intenta aceptar la nueva conexión entrante*/
void
pop3filter_passive_accept(struct selector_key *key) {
    struct sockaddr_storage       client_addr;
    socklen_t                     client_addr_len = sizeof(client_addr);
    struct pop3filter             *state          = NULL;

    const int client = accept(key->fd, (struct sockaddr*) &client_addr,
                                                          &client_addr_len);
    if(client == -1) {
        goto fail;
    }
    if(selector_fd_set_nio(client) == -1) {
        goto fail;
    }
    state = pop3filter_new(client);
    if(state == NULL) {
        // sin un estado, nos es imposible manejaro.
        // tal vez deberiamos apagar accept() hasta que detectemos
        // que se liberó alguna conexión.
        goto fail;
    }
    memcpy(&state->client_addr, &client_addr, client_addr_len);
    state->client_addr_len = client_addr_len;
    /* TODO
    if(SELECTOR_SUCCESS != selector_register(key->s, client, &pop3filter_handler,
                                              OP_READ, state)) {
        goto fail;
    }*/
    return ;
fail:
    if(client != -1) {
        close(client);
    }
    pop3filter_destroy(state);
}
