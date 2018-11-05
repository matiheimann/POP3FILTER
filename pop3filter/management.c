#include <stdio.h>
#include <stdlib.h>  // malloc
#include <string.h>  // memset
#include <assert.h>  // assert
#include <errno.h>
#include <unistd.h>  // close
#include <arpa/inet.h>
#include <netdb.h>
#include <netinet/sctp.h>
#include <sys/types.h>
#include <sys/socket.h>

#include "management.h"
#include "selector.h"
#include "buffer.h"
#include "options.h"
#include "pop3FMP.h"

#define N(x) (sizeof(x)/sizeof((x)[0]))

/** obtiene el struct (management *) desde la llave de selección  */
#define ATTACHMENT(key) ( (struct management *)(key)->data)

struct management {
    /** información del cliente */
    struct sockaddr_storage       client_addr;
    socklen_t                     client_addr_len;
    int                           client_fd;
    
    /** buffers para ser usados read_buffer, write_buffer.*/
    uint8_t raw_buff_a[2048], raw_buff_b[2048]; // TODO: definir tamaño
    buffer read_buffer, write_buffer;
};

/** crea un nuevo `struct management' */
static struct management *
management_new(int client_fd) {
    struct management *ret;

    ret = malloc(sizeof(*ret));

    if(ret == NULL) {
        goto finally;
    }
    memset(ret, 0x00, sizeof(*ret));

    ret->client_fd       = client_fd;
    ret->client_addr_len = sizeof(ret->client_addr);

    buffer_init(&ret->read_buffer,  N(ret->raw_buff_a), ret->raw_buff_a);
    buffer_init(&ret->write_buffer, N(ret->raw_buff_b), ret->raw_buff_b);

finally:
    return ret;
}

static void
management_destroy(struct management* m) {
    free(m);
}

/* declaración forward de los handlers de selección de una conexión
 * establecida entre un cliente y el proxy.
 */

static void management_read   (struct selector_key *key);
static void management_write  (struct selector_key *key);
static void management_close  (struct selector_key *key);
static const struct fd_handler management_handler = {
    .handle_read   = management_read,
    .handle_write  = management_write,
    .handle_close  = management_close,
    .handle_block  = NULL,
};

/* Intenta aceptar la nueva conexión entrante*/
void
management_passive_accept(struct selector_key *key) {
    struct sockaddr_storage       client_addr;
    socklen_t                     client_addr_len = sizeof(client_addr);
    struct management             *state          = NULL;

    const int client = accept(key->fd, (struct sockaddr*) &client_addr,
                                                          &client_addr_len);
    if(client == -1) {
        goto fail;
    }
    if(selector_fd_set_nio(client) == -1) {
        goto fail;
    }
    state = management_new(client);
    if(state == NULL) {
        // sin un estado, nos es imposible manejaro.
        // tal vez deberiamos apagar accept() hasta que detectemos
        // que se liberó alguna conexión.
        goto fail;
    }
    memcpy(&state->client_addr, &client_addr, client_addr_len);
    state->client_addr_len = client_addr_len;

    if(SELECTOR_SUCCESS != selector_register(key->s, client, &management_handler,
                                              OP_READ, state)) {
        goto fail;
    }
    return ;
fail:
    if(client != -1) {
        close(client);
    }
    management_destroy(state);
}

///////////////////////////////////////////////////////////////////////////////
// Handlers top level de la conexión pasiva.
// son los que emiten los eventos a la maquina de estados.

static void
management_read(struct selector_key *key) {
	struct management * data = ATTACHMENT(key);

	uint8_t *ptr;
	size_t  count;
    ssize_t  n;

    ptr = buffer_write_ptr(&data->read_buffer, &count);
    n = sctp_recvmsg(key->fd, ptr, count, NULL, 0, 0, 0);

    if(n > 0) {
        buffer_write_adv(&data->read_buffer, n);
    }

	if(SELECTOR_SUCCESS != selector_set_interest_key(key, OP_WRITE)) {
       selector_unregister_fd(key->s, data->client_fd);
    }
}

static void
management_write(struct selector_key *key) {
	struct management * data = ATTACHMENT(key);

    uint8_t *response = (uint8_t*) calloc(MAX_RESPONSE, sizeof(uint8_t));
    int size;
    ssize_t  n;

    response = receivePOP3FMPRequest(&data->read_buffer, response, &size);
    n = sctp_sendmsg(key->fd, response, size, NULL, 0, 0, 0, 0, 0, 0);
    if(n == 0)
    {
        // TODO: register error.
    }
    free(response);
    if(SELECTOR_SUCCESS != selector_set_interest_key(key, OP_READ)) {
       selector_unregister_fd(key->s, data->client_fd);
    }
}

static void
management_close(struct selector_key *key) {
    management_destroy(ATTACHMENT(key));
}
