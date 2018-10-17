#include <stdio.h>
#include <stdlib.h>  // malloc
#include <string.h>  // memset
#include <strings.h> // strncasecmp
#include <assert.h>  // assert
#include <errno.h>
#include <unistd.h>  // close
#include <arpa/inet.h>
#include <netdb.h>
#include <sys/wait.h>
#include <pthread.h>

#include "pop3filter.h"
#include "selector.h"
#include "stm.h"
#include "buffer.h"
#include "options.h"

#define N(x) (sizeof(x)/sizeof((x)[0]))

/** obtiene el struct (pop3filter *) desde la llave de selección  */
#define ATTACHMENT(key) ( (struct pop3filter *)(key)->data)

/** maquina de estados general */
enum pop3filter_state {
    /**
     *  Inicia la conexion con el origin server
     *
     *  Transiciones:
     *      - CONNECTING    una vez que se inicia la conexion
     *      - ERROR         si no pudo iniciar la conexion
     *
     */
            CONNECT,
    /**
     *  Espera que se establezca la conexion con el origin server
     *
     *  Transiciones:
     *      - HELLO    una vez que se establecio la conexion
     *      - ERROR      si no se pudo conectar
     */
            CONNECTING,
    /**
     *  Lee el mensaje de bienvenida del origin server
     *
     *  Transiciones:
     *      - HELLO         mientras el mensaje no este completo
     *      - ERROR         ante cualquier error (IO/parseo)
     */
            HELLO,
    /**
     *  Lee requests del cliente y las manda al origin server
     *
     *  Transiciones:
     *      - REQUEST       mientras la request no este completa
     *      - RESPONSE      cuando la request esta completa
     *      - ERROR         ante cualquier error (IO/parseo)
     */
            REQUEST,
    /**
     *  Lee la respuesta del origin server y se la envia al cliente
     *
     *  Transiciones:
     *      - RESPONSE                  mientras la respuesta no este completa
     *      - FILTER                    si la request era un RETR
     *      - REQUEST                   cuando la respuesta esta completa
     *      - ERROR                     ante cualquier error (IO/parseo)
     */
            RESPONSE,
    /**
     *  Filtra el mail del origin server y se lo envia al cliente
     *
     *  Transiciones:
     *      - FILTER                    mientras el filtrado no este completo
     *      - RESPONSE                  cuando el filtrado esta completo
     *      - ERROR                     ante cualquier error (IO/parseo)
     */
            FILTER,
    // estados terminales
            DONE,
            ERROR,
};

////////////////////////////////////////////////////////////////////
// Definición de variables para cada estado

/** usado por HELLO */
struct hello_st {
    /** buffer utilizado para I/O */
    buffer * wb;
};

/** usado por REQUEST */
struct request_st {
    /** buffer utilizado para I/O */
    buffer                      *rb, *wb;

    /* TODO
    // parser
    struct request              request;
    struct request_parser       request_parser;
    */
};

/** usado por RESPONSE */
struct response_st {
    buffer                      *rb, *wb;
    int isMail;

    /* TODO
    struct request              *request;
    struct response_parser      response_parser;
    */
};

/** usado por FILTER */
struct filter_st {
    /** buffer utilizado para I/O */
    buffer                      *rb, *wb;
};

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

    /** información del origin server */
    struct sockaddr_storage       origin_addr;
    socklen_t                     origin_addr_len;
    int                           origin_domain;
    int                           origin_fd;

    int filter_in_fds[2];
    int filter_out_fds[2];

    /** maquinas de estados */
    struct state_machine          stm;

    /** estados para el client_fd */
    union {
        struct request_st         request;
    } client;

    /** estados para el filter_fd */
    union {
        struct filter_st          filter;
    } filter;

    /** estados para el origin_fd */
    union {
        struct hello_st           hello;
        struct response_st        response;
    } orig;
    
    /** buffers para ser usados read_buffer, write_buffer.*/
    uint8_t raw_buff_a[2048], raw_buff_b[2048]; // TODO: definir tamaño
    buffer read_buffer, write_buffer;
    
    /** cantidad de referencias a este objeto. si es uno se debe destruir */
    unsigned references;

    /** siguiente en el pool */
    struct pop3filter *next;
};

static const unsigned  max_pool  = 50; // TODO: tamaño máximo
static unsigned        pool_size = 0;  // tamaño actual
static struct pop3filter * pool  = 0;  // pool propiamente dicho

static const struct state_definition *
pop3filter_describe_states(void);

/** crea un nuevo `struct pop3filter' */
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

    ret->filter_in_fds[0] = -1;
    ret->filter_in_fds[1] = -1;
    ret->filter_out_fds[0] = -1;
    ret->filter_out_fds[1] = -1;

    ret->stm    .initial   = CONNECT;
    ret->stm    .max_state = ERROR;
    ret->stm    .states    = pop3filter_describe_states();
    stm_init(&ret->stm);

    buffer_init(&ret->read_buffer,  N(ret->raw_buff_a), ret->raw_buff_a);
    buffer_init(&ret->write_buffer, N(ret->raw_buff_b), ret->raw_buff_b);

    ret->references = 1;
finally:
    return ret;
}

/** realmente destruye */
static void
pop3filter_destroy_(struct pop3filter* pop3) {
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

static void pop3filter_read   (struct selector_key *key);
static void pop3filter_write  (struct selector_key *key);
static void pop3filter_block  (struct selector_key *key);
static void pop3filter_close  (struct selector_key *key);
static const struct fd_handler pop3filter_handler = {
    .handle_read   = pop3filter_read,
    .handle_write  = pop3filter_write,
    .handle_close  = pop3filter_close,
    .handle_block  = pop3filter_block,
};

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

    if(SELECTOR_SUCCESS != selector_register(key->s, client, &pop3filter_handler,
                                              OP_WRITE, state)) {
        goto fail;
    }
    return ;
fail:
    if(client != -1) {
        close(client);
    }
    pop3filter_destroy(state);
}

////////////////////////////////////////////////////////////////////////////////
// CONNECT
////////////////////////////////////////////////////////////////////////////////

static void *
connect_resolv_blocking(void *data);

// Comienza la resolucion de nombre en un thread aparte.
static unsigned
connect_start(struct selector_key *key) {
    enum pop3filter_state  ret;
    pthread_t tid;

    struct selector_key* k = malloc(sizeof(*key));
    if(k == NULL) {
        perror("resolv() failed");
        ret = ERROR;
    }
    else {
        memcpy(k, key, sizeof(*k));
        if(-1 == pthread_create(&tid, 0, connect_resolv_blocking, k)) {
            perror("resolv() failed");
            ret = ERROR;
        } 
        else  {
            ret = CONNECT;
            selector_set_interest_key(key, OP_NOOP);
        }
    }

    return ret;
}

/**
 * Realiza la resolución de DNS bloqueante.
 *
 * Una vez resuelto notifica al selector para que el evento esté
 * disponible en la próxima iteración.
 */
static void *
connect_resolv_blocking(void *data) {
    struct selector_key *key = (struct selector_key *) data;
    struct pop3filter       *p   = ATTACHMENT(key);

    pthread_detach(pthread_self());
    p->origin_resolution = 0;
    struct addrinfo hints = {
        .ai_family    = AF_UNSPEC,    /* Allow IPv4 or IPv6 */
        .ai_socktype  = SOCK_STREAM,  /* Datagram socket */
        .ai_flags     = AI_PASSIVE,   /* For wildcard IP address */
        .ai_protocol  = 0,            /* Any protocol */
        .ai_canonname = NULL,
        .ai_addr      = NULL,
        .ai_next      = NULL,
    };

    char buff[7];
    snprintf(buff, sizeof(buff), "%d", options->originPort);

    getaddrinfo(options->originServer, buff, &hints,
               &p->origin_resolution);

    selector_notify_block(key->s, key->fd);

    free(data);

    return 0;
}

static unsigned
connect_resolv_done(struct selector_key *key)
{
    struct pop3filter       *p   = ATTACHMENT(key);

    if(p->origin_resolution == 0) {
        perror("resolv() failed");
        goto error;
    }

    p->origin_domain   = p->origin_resolution->ai_family;
    p->origin_addr_len = p->origin_resolution->ai_addrlen;
    memcpy(&p->origin_addr,
            p->origin_resolution->ai_addr,
            p->origin_resolution->ai_addrlen);
    freeaddrinfo(p->origin_resolution);
    p->origin_resolution = 0;

    int sock = socket(p->origin_domain, SOCK_STREAM, IPPROTO_TCP);

    if (sock < 0) {
        perror("socket() failed");
        goto error;
    }

    if (selector_fd_set_nio(sock) == -1) {
        goto error;
    }

    /* Establish the connection to the origin server */
    if (-1 == connect(sock, 
                    (const struct sockaddr *)&p->origin_addr,
                    p->origin_addr_len)) {
        if(errno == EINPROGRESS) {
            // como es no bloqueante tiene que entrar aca

            // dejamos de pollear el socket del cliente
            selector_status st = selector_set_interest_key(key, OP_NOOP);
            if(SELECTOR_SUCCESS != st) {
                goto error;
            }

            // esperamos la conexion en el nuevo socket
            st = selector_register(key->s, sock, &pop3filter_handler,
                                   OP_WRITE, key->data);
            if(SELECTOR_SUCCESS != st) {
                goto error;
            }
            ATTACHMENT(key)->references += 1;
        } else {
            goto error;
        }
    } else {
        goto error;
    }

    return CONNECTING;

error:
    if (sock != -1) {
        close(sock);
    }
    return ERROR;
}

////////////////////////////////////////////////////////////////////////////////
// CONNECTING
////////////////////////////////////////////////////////////////////////////////

static void
connecting_init(const unsigned state, struct selector_key *key) 
{

}

static unsigned
connecting(struct selector_key *key) 
{
    int error;
    socklen_t len = sizeof(error);
    struct pop3filter *d = ATTACHMENT(key);

    d->origin_fd = key->fd;

    if (getsockopt(key->fd, SOL_SOCKET, SO_ERROR, &error, &len) < 0) {
        const char * msg = "-ERR Connection failed.\r\n";
        send(d->client_fd, msg, strlen(msg), 0);
        fprintf(stderr, "Connection to origin server failed\n");
        selector_set_interest_key(key, OP_NOOP);
        return ERROR;
    } else {
        if(error == 0) {
            d->origin_fd = key->fd;
        } else {
            const char * msg = "-ERR Connection failed.\r\n";
            send(d->client_fd, msg, strlen(msg), 0);
            fprintf(stderr, "Connection to origin server failed\n");
            selector_set_interest_key(key, OP_NOOP);
            return ERROR;
        }
    }

    selector_status ss = SELECTOR_SUCCESS;

    ss |= selector_set_interest_key(key, OP_READ);
    ss |= selector_set_interest(key->s, ATTACHMENT(key)->client_fd, OP_NOOP);

    return SELECTOR_SUCCESS == ss ? HELLO : ERROR;
}

////////////////////////////////////////////////////////////////////////////////
// HELLO
////////////////////////////////////////////////////////////////////////////////

/** inicializa las variables del estado HELLO */
static void
hello_init(const unsigned state, struct selector_key *key) 
{
    struct hello_st *d = &ATTACHMENT(key)->orig.hello;
    d->wb = &ATTACHMENT(key)->write_buffer;
}

/** Lee todos los bytes del mensaje de tipo `hello' de server_fd */
static unsigned
hello_read(struct selector_key *key) 
{
    struct hello_st *d      = &ATTACHMENT(key)->orig.hello;
    enum pop3filter_state  ret    = HELLO;
    uint8_t *ptr;
    size_t  count;
    ssize_t  n;

    /*
    ptr = buffer_write_ptr(d->wb, &count);
    const char * msg = "+OK pop3filter ready.\r\n";
    n = strlen(msg);
    strcpy((char *) ptr, msg);
    buffer_write_adv(d->wb, n);
    */

    ptr = buffer_write_ptr(d->wb, &count);
    n = recv(key->fd, ptr, count, 0);

    if(n > 0) {
        buffer_write_adv(d->wb, n);
        selector_status ss = SELECTOR_SUCCESS;
        ss |= selector_set_interest_key(key, OP_NOOP);
        ss |= selector_set_interest(key->s, ATTACHMENT(key)->client_fd, OP_WRITE);
        if (ss != SELECTOR_SUCCESS) {
            ret = ERROR;
        }
    } else {
        ret = ERROR;
    }

    return ret;
}

/** Escribe todos los bytes del mensaje `hello' en client_fd */
static unsigned
hello_write(struct selector_key *key) 
{
    struct hello_st *d = &ATTACHMENT(key)->orig.hello;

    enum pop3filter_state  ret      = HELLO;
    uint8_t *ptr;
    size_t  count;
    ssize_t  n;

    ptr = buffer_read_ptr(d->wb, &count);
    n = send(key->fd, ptr, count, MSG_NOSIGNAL);

    if(n == -1) {
        ret = ERROR;
    } else {
        buffer_read_adv(d->wb, n);
        if(!buffer_can_read(d->wb)) {
            selector_status ss = SELECTOR_SUCCESS;
            ss |= selector_set_interest_key(key, OP_NOOP);
            ss |= selector_set_interest(key->s, ATTACHMENT(key)->client_fd, OP_READ);
            ret = SELECTOR_SUCCESS == ss ? REQUEST : ERROR;
        }
    }

    return ret;
}

static void
hello_close(const unsigned state, struct selector_key *key) {
    
}

////////////////////////////////////////////////////////////////////////////////
// REQUEST
////////////////////////////////////////////////////////////////////////////////

/** inicializa las variables del estado REQUEST */
static void
request_init(const unsigned state, struct selector_key *key) 
{
    struct request_st * d = &ATTACHMENT(key)->client.request;
    d->rb              = &ATTACHMENT(key)->read_buffer;
    d->wb              = &ATTACHMENT(key)->write_buffer;
}

/** Lee la request del cliente */
static unsigned
request_read(struct selector_key *key) 
{
    struct request_st *d = &ATTACHMENT(key)->client.request;
    enum pop3filter_state ret  = REQUEST;

    buffer *b            = d->rb;
    uint8_t *ptr;
    size_t  count;
    ssize_t  n;

    ptr = buffer_write_ptr(b, &count);
    n = recv(key->fd, ptr, count, 0);

    if(n > 0 || buffer_can_read(b)) {
        buffer_write_adv(b, n);
        selector_status ss = SELECTOR_SUCCESS;
        ss |= selector_set_interest_key(key, OP_NOOP);
        ss |= selector_set_interest(key->s, ATTACHMENT(key)->origin_fd, OP_WRITE);
        ret = SELECTOR_SUCCESS == ss ? REQUEST : ERROR;
    } else {
        ret = ERROR;
    }

    return ret;
}

/** Escribe la request en el server */
static unsigned
request_write(struct selector_key *key) 
{
    struct request_st *d = &ATTACHMENT(key)->client.request;
    enum pop3filter_state ret      = REQUEST;

    buffer *b          = d->rb;
    uint8_t *ptr;
    size_t  count;
    ssize_t  n;

    ptr = buffer_read_ptr(b, &count);
    n = send(key->fd, ptr, count, MSG_NOSIGNAL);

    if(n == -1) {
        ret = ERROR;
    } else {
        if (strncasecmp((char*)ptr, "RETR ", 5) == 0)
            ATTACHMENT(key)->orig.response.isMail = 1;
        else
            ATTACHMENT(key)->orig.response.isMail = 0;
        buffer_read_adv(b, n);
        if(!buffer_can_read(b)) {
            if(SELECTOR_SUCCESS == selector_set_interest_key(key, OP_READ)) {
                ret = RESPONSE;
            } else {
                ret = ERROR;
            }
        }
    }

    return ret;
}

static void
request_close(const unsigned state, struct selector_key *key) 
{

}

////////////////////////////////////////////////////////////////////////////////
// RESPONSE
////////////////////////////////////////////////////////////////////////////////

/** inicializa las variables del estado RESPONSE */
static void
response_init(const unsigned state, struct selector_key *key) 
{
    struct response_st * d = &ATTACHMENT(key)->orig.response;
    d->rb                       = &ATTACHMENT(key)->read_buffer;
    d->wb                       = &ATTACHMENT(key)->write_buffer;
}

/** Lee la respuesta del origin server */
static unsigned
response_read(struct selector_key *key) 
{
    struct response_st *d = &ATTACHMENT(key)->orig.response;
    enum pop3filter_state ret      = RESPONSE;

    buffer  *b         = d->rb;
    uint8_t *ptr;
    size_t  count;
    ssize_t  n;

    ptr = buffer_write_ptr(b, &count);
    n = recv(key->fd, ptr, count, 0);

    if(n > 0 || buffer_can_read(b)) {
        buffer_write_adv(b, n);
        if (d->isMail) {
            if (pipe(ATTACHMENT(key)->filter_in_fds) == -1 ||
                pipe(ATTACHMENT(key)->filter_out_fds) == -1 ||
                    selector_fd_set_nio(ATTACHMENT(key)->filter_in_fds[1]) == -1 ||
                    selector_fd_set_nio(ATTACHMENT(key)->filter_out_fds[0]) == -1) {
                ret = ERROR;
            } 
            else {
                selector_status ss = SELECTOR_SUCCESS;
                ss |= selector_set_interest_key(key, OP_NOOP);
                ss |= selector_register(key->s, ATTACHMENT(key)->filter_in_fds[1], &pop3filter_handler,
                                   OP_WRITE, key->data);
                ret = ss == SELECTOR_SUCCESS ? FILTER : ERROR;
            }
        }
        else {
            selector_status ss = SELECTOR_SUCCESS;
            ss |= selector_set_interest_key(key, OP_NOOP);
            ss |= selector_set_interest(key->s, ATTACHMENT(key)->client_fd, OP_WRITE);
            ret = ss == SELECTOR_SUCCESS ? RESPONSE : ERROR;
        }
    } else {
        ret = ERROR;
    }

    //filterMail(key->s, "Test", "/bin/cat");

    return ret;
}

/** Escribe la respuesta en el cliente */
static unsigned
response_write(struct selector_key *key) 
{
    struct response_st *d = &ATTACHMENT(key)->orig.response;
    enum pop3filter_state ret = RESPONSE;

    buffer  *b         = d->rb;
    uint8_t *ptr;
    size_t  count;
    ssize_t  n;

    ptr = buffer_read_ptr(b, &count);
    n = send(key->fd, ptr, count, MSG_NOSIGNAL);

    if(n == -1) {
        ret = ERROR;
    } else {
        buffer_read_adv(b, n);
        if (!buffer_can_read(b)) {
            if (d->isMail) {
                selector_unregister_fd(key->s, ATTACHMENT(key)->filter_in_fds[1]);
                selector_unregister_fd(key->s, ATTACHMENT(key)->filter_out_fds[0]);
                ATTACHMENT(key)->filter_in_fds[0] = -1;
                ATTACHMENT(key)->filter_in_fds[1] = -1;
                ATTACHMENT(key)->filter_out_fds[0] = -1;
                ATTACHMENT(key)->filter_out_fds[1] = -1;
                wait(NULL);
            }

            selector_status ss = SELECTOR_SUCCESS;
            ss |= selector_set_interest_key(key, OP_NOOP);
            ss |= selector_set_interest(key->s, ATTACHMENT(key)->client_fd, OP_READ);
            ret = SELECTOR_SUCCESS == ss ? REQUEST : ERROR;
        }
    }

    return ret;
}

static void
response_close(const unsigned state, struct selector_key *key) 
{

}

////////////////////////////////////////////////////////////////////////////////
// FILTER
////////////////////////////////////////////////////////////////////////////////

/** inicializa las variables del estado FILTER */
static void
filter_init(const unsigned state, struct selector_key *key) 
{
    struct filter_st * d = &ATTACHMENT(key)->filter.filter;
    d->rb              = &ATTACHMENT(key)->read_buffer;
    d->wb              = &ATTACHMENT(key)->write_buffer;
}

/** Lee el mail filtrado del stdout del filter */
static unsigned
filter_read(struct selector_key *key) 
{
    struct filter_st *d = &ATTACHMENT(key)->filter.filter;
    enum pop3filter_state ret      = FILTER;

    buffer  *b         = d->rb;
    uint8_t *ptr;
    size_t  count;
    ssize_t  n;

    ptr = buffer_write_ptr(b, &count);
    const char * msg = "THE NEXT MAIL HAS PASSED THROUGH THE FILTER:\n";
    n = strlen(msg);
    strcpy((char*)ptr, msg);
    buffer_write_adv(b, n);
    ptr = buffer_write_ptr(b, &count);
    n = read(key->fd, ptr, count);

    if(n > 0 || buffer_can_read(b)) {
        buffer_write_adv(b, n);
        selector_status ss = SELECTOR_SUCCESS;
        ss |= selector_set_interest_key(key, OP_NOOP);
        ss |= selector_set_interest(key->s, ATTACHMENT(key)->client_fd, OP_WRITE);
        ret = ss == SELECTOR_SUCCESS ? RESPONSE : ERROR;
    } else {
        ret = ERROR;
    }

    return ret;
}

/** Escribe el mail en el stdin del filter */
static unsigned
filter_write(struct selector_key *key) 
{
    struct filter_st *d = &ATTACHMENT(key)->filter.filter;
    enum pop3filter_state ret = FILTER;

    buffer  *b         = d->rb;
    uint8_t *ptr;
    size_t  count;
    ssize_t  n;

    ptr = buffer_read_ptr(b, &count);
    n = write(key->fd, ptr, count);

    if(n == -1) {
        ret = ERROR;
    } else {
        buffer_read_adv(b, n);
        if (!buffer_can_read(b)) {
            int pid = fork();
            if (pid == -1) {
                ret = ERROR;
            }
            else if (pid == 0) {
                close(ATTACHMENT(key)->filter_in_fds[1]);
                close(ATTACHMENT(key)->filter_out_fds[0]);

                dup2(ATTACHMENT(key)->filter_in_fds[0], STDIN_FILENO);
                dup2(ATTACHMENT(key)->filter_out_fds[1], STDOUT_FILENO);
        
                execl("/bin/sh", "sh", "-c", "cat", NULL);
            }
            else {
                close(ATTACHMENT(key)->filter_in_fds[0]);
                close(ATTACHMENT(key)->filter_in_fds[1]);
                close(ATTACHMENT(key)->filter_out_fds[1]);

                selector_status ss = SELECTOR_SUCCESS;
                ss |= selector_set_interest_key(key, OP_NOOP);
                ss |= selector_register(key->s, ATTACHMENT(key)->filter_out_fds[0], &pop3filter_handler,
                                   OP_READ, key->data);
                ret = ss == SELECTOR_SUCCESS ? FILTER : ERROR;
            }
        }
    }

    return ret;
}

static void
filter_close(const unsigned state, struct selector_key *key) 
{

}

/** definición de handlers para cada estado */
static const struct state_definition client_statbl[] = {
    {
        .state            = CONNECT,
        .on_write_ready   = connect_start,
        .on_block_ready   = connect_resolv_done,
    },{
        .state            = CONNECTING,
        .on_arrival       = connecting_init,
        .on_write_ready   = connecting,
    },{
        .state            = HELLO,
        .on_arrival       = hello_init,
        .on_read_ready    = hello_read,
        .on_write_ready   = hello_write,
        .on_departure     = hello_close,
    },{
        .state            = REQUEST,
        .on_arrival       = request_init,
        .on_read_ready    = request_read,
        .on_write_ready   = request_write,
        .on_departure     = request_close,
    },{
        .state            = RESPONSE,
        .on_arrival       = response_init,
        .on_read_ready    = response_read,
        .on_write_ready   = response_write,
        .on_departure     = response_close,
    },{
        .state            = FILTER,
        .on_arrival       = filter_init,
        .on_read_ready    = filter_read,
        .on_write_ready   = filter_write,
        .on_departure     = filter_close,
    },{
        .state            = DONE,
    },{
        .state            = ERROR,
    }
};

static const struct state_definition *
pop3filter_describe_states(void) {
    return client_statbl;
}

///////////////////////////////////////////////////////////////////////////////
// Handlers top level de la conexión pasiva.
// son los que emiten los eventos a la maquina de estados.
static void
pop3filter_done(struct selector_key *key);

static void
pop3filter_read(struct selector_key *key) {
    struct state_machine *stm   = &ATTACHMENT(key)->stm;
    const enum pop3filter_state st    = (enum pop3filter_state)stm_handler_read(stm, key);

    if(ERROR == st || DONE == st) {
        pop3filter_done(key);
    }
}

static void
pop3filter_write(struct selector_key *key) {
    struct state_machine *stm   = &ATTACHMENT(key)->stm;
    const enum pop3filter_state st    = (enum pop3filter_state)stm_handler_write(stm, key);

    if(ERROR == st || DONE == st) {
        pop3filter_done(key);
    }
}

static void
pop3filter_block(struct selector_key *key) {
    struct state_machine *stm   = &ATTACHMENT(key)->stm;
    const enum pop3filter_state st    = (enum pop3filter_state)stm_handler_block(stm, key);

    if(ERROR == st || DONE == st) {
        pop3filter_done(key);
    }
}

static void
pop3filter_close(struct selector_key *key) {
    pop3filter_destroy(ATTACHMENT(key));
}

static void
pop3filter_done(struct selector_key *key) {
    const int fds[] = {
            ATTACHMENT(key)->client_fd,
            ATTACHMENT(key)->origin_fd,
            ATTACHMENT(key)->filter_in_fds[0],
            ATTACHMENT(key)->filter_in_fds[1],
            ATTACHMENT(key)->filter_out_fds[0],
            ATTACHMENT(key)->filter_out_fds[1],
    };
    for(unsigned i = 0; i < N(fds); i++) {
        if(fds[i] != -1) {
            if(SELECTOR_SUCCESS != selector_unregister_fd(key->s, fds[i])) {
                abort();
            }
            close(fds[i]);
        }
    }
}