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
#include <poll.h>

#include "pop3filter.h"
#include "selector.h"
#include "stm.h"
#include "buffer.h"
#include "pop3_multi.h"
#include "options.h"
#include "metrics.h"
#include "iputils.h"
#include "timeutils.h"
#include "logging_lib.h"

#define N(x) (sizeof(x)/sizeof((x)[0]))

/** obtiene el struct (pop3filter *) desde la llave de selección  */
#define ATTACHMENT(key) ( (struct pop3filter *)(key)->data)

#define MAX_BUFFER 1024

/** maquina de estados general */
enum pop3filter_state {
    /**
     *  Inicia la conexion con el origin server
     *
     *  Transiciones:
     *      - CONNECTING                una vez que se inicia la conexion
     *      - ERROR                     si no pudo iniciar la conexion
     *
     */
            CONNECT,
    /**
     *  Espera que se establezca la conexion con el origin server
     *
     *  Transiciones:
     *      - HELLO                     una vez que se establecio la conexion
     *      - ERROR                     si no se pudo conectar
     */
            CONNECTING,
    /**
     *  Lee el mensaje de bienvenida del origin server
     *
     *  Transiciones:
     *      - HELLO                     mientras el mensaje no este completo
     *      - CAPA                      cuando el hello este completo
     *      - ERROR                     ante cualquier error (IO/parseo)
     */
            HELLO,
    /**
     *  Lee las capacidades del origin server para saber si soporta pipelining
     *
     *  Transiciones:
     *      - CAPA                      mientras el mensaje no este completo
     *      - REQUEST                   cuando el capa este completo
     *      - ERROR                     ante cualquier error (IO/parseo)
     */
            CAPA,
    /**
     *  Lee requests del cliente y las manda al origin server
     *
     *  Transiciones:
     *      - REQUEST                   mientras la request no este completa
     *      - RESPONSE                  cuando la request esta completa
     *      - ERROR                     ante cualquier error (IO/parseo)
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

enum request_cmd_type {
            MULTI_RETR,
            MULTI_LIST,
            MULTI_CAPA,
            MULTI_UIDL,
            MULTI_TOP,
            USER,
            PASS,
            DELE,
            QUIT,
            DEFAULT,
};

struct next_request {
    enum request_cmd_type       cmd_type;
    struct next_request         *next;
};

/** usado por HELLO */
struct hello_st {
    /** buffer utilizado para I/O */
    buffer                      *resb, *stab;
};

/** usado por REQUEST */
struct request_st {
    /** buffer utilizado para I/O */
    buffer                      *reqb, *auxb, *cmdb;
    enum request_cmd_type       cmd_type;

    struct next_request         *next_cmd_type;
    struct next_request         *last_cmd_type;
};

/** usado por RESPONSE */
struct response_st {
    /** buffer utilizado para I/O */
    buffer                      *resb, *filb, *stab;
    struct parser               *multi_parser;
};

/** usado por FILTER */
struct filter_st {
    /** buffer utilizado para I/O */
    buffer                      *resb, *filb;
    struct parser               *multi_parser;
};

struct env_st {
    char * filterMedias;
    char * filterMessage;
    char * pop3filter_version;
    char * pop3_username;
    char * pop3_server;
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

    /** información de accesos durante la sesión */
    char*                         logged_in_username;
    int                           top_commands;
    int                           retr_commands;
    int                           dele_commands;

    /** resolución de la dirección del origin server */
    struct addrinfo              *origin_resolution;

    /** información del origin server */
    struct sockaddr_storage       origin_addr;
    socklen_t                     origin_addr_len;
    int                           origin_domain;
    int                           origin_fd;
    bool                          origin_pipelining;

    /** información del proceso filter */
    int                           filter_in_fds[2];
    int                           filter_out_fds[2];
    int                           filter_pid;

    /** maquinas de estados */
    struct state_machine          stm;

    /** estados para el client_fd */
    union {
        struct request_st         request;
    } client;

    /** estados para el origin_fd */
    union {
        struct hello_st           hello;
        struct response_st        response;
    } orig;

    /** estados para el filter_fd */
    union {
        struct filter_st          filter;
    } filter;

    struct env_st                 env_variables;
    
    /** buffers para ser usados read_buffer, write_buffer.*/
    uint8_t raw_buff_a[MAX_BUFFER], raw_buff_b[MAX_BUFFER], raw_buff_c[MAX_BUFFER], raw_buff_d[MAX_BUFFER], raw_buff_e[MAX_BUFFER], raw_buff_f[MAX_BUFFER];
    buffer request_buffer, response_buffer, filter_buffer, aux_request_buffer, cmd_request_buffer, status_response_buffer;
    
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
    ret->filter_pid = -1;

    ret->stm    .initial   = CONNECT;
    ret->stm    .max_state = ERROR;
    ret->stm    .states    = pop3filter_describe_states();
    stm_init(&ret->stm);

    buffer_init(&ret->request_buffer,  N(ret->raw_buff_a), ret->raw_buff_a);
    buffer_init(&ret->response_buffer,  N(ret->raw_buff_b), ret->raw_buff_b);
    buffer_init(&ret->filter_buffer,  N(ret->raw_buff_c), ret->raw_buff_c);
    buffer_init(&ret->aux_request_buffer,  N(ret->raw_buff_d), ret->raw_buff_d);
    buffer_init(&ret->cmd_request_buffer,  N(ret->raw_buff_e), ret->raw_buff_e);
    buffer_init(&ret->status_response_buffer,  N(ret->raw_buff_f), ret->raw_buff_f);

    metrics->concurrentConnections++;
    metrics->historicConnections++;

    ret->references = 1;
finally:
    return ret;
}

/** realmente destruye */
static void
pop3filter_destroy_(struct pop3filter* pop3) {
    if(pop3->origin_resolution != NULL) {
        freeaddrinfo(pop3->origin_resolution);
        pop3->origin_resolution = 0;
    }
    if(pop3->logged_in_username != NULL) {
        free(pop3->logged_in_username);
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
    print_error_message("error accepting client connection request");
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
        ret = ERROR;
    }
    else {
        memcpy(k, key, sizeof(*k));
        if(-1 == pthread_create(&tid, 0, connect_resolv_blocking, k)) {
            ret = ERROR;
        } 
        else  {
            ret = CONNECT;
            selector_set_interest_key(key, OP_NOOP);
        }
    }

    if(ret == ERROR) {
        print_error_message("error resolving origin name server");
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
    int error_connecting_to_origin = 0;
    int error_creating_socket = 0;

    if(p->origin_resolution == 0) {
        print_error_message("error resolving origin name server");
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
        error_creating_socket = 1;
        goto error;
    }

    if (selector_fd_set_nio(sock) == -1) {
        error_creating_socket = 1;
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
                error_connecting_to_origin = 1;
                goto error;
            }

            // esperamos la conexion en el nuevo socket
            st = selector_register(key->s, sock, &pop3filter_handler,
                                   OP_WRITE, key->data);
            if(SELECTOR_SUCCESS != st) {
                error_connecting_to_origin = 1;
                goto error;
            }
            ATTACHMENT(key)->references += 1;
        } else {
            error_connecting_to_origin = 1;
            goto error;
        }
    } else {
        error_connecting_to_origin = 1;
        goto error;
    }

    return CONNECTING;

error:
    if (sock != -1) {
        close(sock);
    }
    if(error_creating_socket) {
        print_error_message("error creating socket to communicate with origin server");
    }
    if(error_connecting_to_origin) {
        print_error_message("error connecting to origin server");
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
        const char * msg = "-ERR Connection refused\r\n";
        send(d->client_fd, msg, strlen(msg), 0);
        print_error_message("connection to origin server failed");
        selector_set_interest_key(key, OP_NOOP);
        return ERROR;
    } else {
        if(error == 0) {
            d->origin_fd = key->fd;
        } else {
            const char * msg = "-ERR Connection refused\r\n";
            send(d->client_fd, msg, strlen(msg), 0);
            print_error_message("connection to origin server failed");
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

static ssize_t send_hello_status_line(struct selector_key *key, buffer * b);

/** inicializa las variables del estado HELLO */
static void
hello_init(const unsigned state, struct selector_key *key) 
{
    struct hello_st *d = &ATTACHMENT(key)->orig.hello;
    d->resb = &ATTACHMENT(key)->response_buffer;
    d->stab = &ATTACHMENT(key)->status_response_buffer;
}

/** Lee todos los bytes del mensaje de tipo `hello' de server_fd */
static unsigned
hello_read(struct selector_key *key) 
{
    struct hello_st *d      = &ATTACHMENT(key)->orig.hello;
    enum pop3filter_state  ret    = HELLO;

    buffer *b            = d->resb;
    uint8_t *ptr;
    size_t  count;
    ssize_t  n;

    ptr = buffer_write_ptr(b, &count);
    n = recv(key->fd, ptr, count, 0);

    if(n > 0) {
        buffer_write_adv(b, n);
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

    buffer *b            = d->resb;
    ssize_t  n;

    n = send_hello_status_line(key, b);

    if (n == -1) {
        ret = ERROR;
    } 
    else if (n == 0) {
        selector_status ss = SELECTOR_SUCCESS;
        ss |= selector_set_interest_key(key, OP_NOOP);
        ss |= selector_set_interest(key->s, ATTACHMENT(key)->origin_fd, OP_READ);
        ret = SELECTOR_SUCCESS == ss ? HELLO : ERROR;
    } else {
        selector_status ss = SELECTOR_SUCCESS;
        ss |= selector_set_interest_key(key, OP_NOOP);
        ss |= selector_set_interest(key->s, ATTACHMENT(key)->origin_fd, OP_READ);
        ret = SELECTOR_SUCCESS == ss ? CAPA : ERROR;

        if (ret == CAPA) {
            char * msg = "CAPA\r\n";
            send(ATTACHMENT(key)->origin_fd, msg, strlen(msg), MSG_NOSIGNAL);
        }
    }

    return ret;
}

static ssize_t
send_hello_status_line(struct selector_key *key, buffer * b) {
    buffer *sb            = ATTACHMENT(key)->orig.hello.stab;
    uint8_t *sptr;

    size_t count;
    ssize_t n = 0;

    size_t i = 0;

    while (buffer_can_read(b) && n == 0) {
        i++;
        char c = buffer_read(b);
        if (c == '\n') {
            n = i;
        }
        buffer_write(sb, c);
    }

    if (n == 0) {
        return 0;
    }

    sptr = buffer_read_ptr(sb, &count);

    n = send(ATTACHMENT(key)->client_fd, sptr, count, MSG_NOSIGNAL);

    buffer_reset(sb);

    return n;
}

static void
hello_close(const unsigned state, struct selector_key *key)
{
    
}

////////////////////////////////////////////////////////////////////////////////
// CAPA
////////////////////////////////////////////////////////////////////////////////

/** inicializa las variables del estado HELLO */
static void
capa_init(const unsigned state, struct selector_key *key) 
{
    struct response_st * d     = &ATTACHMENT(key)->orig.response;
    d->resb = &ATTACHMENT(key)->response_buffer;
}

/** Lee todos los bytes de la respuesta de CAPA del origin_fd */
static unsigned
capa_read(struct selector_key *key) 
{
    struct response_st * d     = &ATTACHMENT(key)->orig.response;
    enum pop3filter_state  ret    = CAPA;

    buffer *b            = d->resb;
    uint8_t *ptr;
    size_t  count;
    ssize_t  n;

    ptr = buffer_write_ptr(b, &count);
    n = recv(key->fd, ptr, count, 0);

    if(n > 0) {
        buffer_write_adv(b, n);

        ptr = buffer_read_ptr(b, &count);
        if (strstr((char*)ptr, "PIPELINING"))
            ATTACHMENT(key)->origin_pipelining = true;

        if (!strstr((char*)ptr, "\r\n.\r\n")) {
            selector_status ss = SELECTOR_SUCCESS;
            ss |= selector_set_interest_key(key, OP_NOOP);
            ss |= selector_set_interest(key->s, ATTACHMENT(key)->origin_fd, OP_READ);
            ret = SELECTOR_SUCCESS == ss ? CAPA : ERROR;
        }
        else {
            buffer_reset(b);
            printf("%sHAY PIPELINING\n", ATTACHMENT(key)->origin_pipelining ? "" : "NO ");
            selector_status ss = SELECTOR_SUCCESS;
            ss |= selector_set_interest_key(key, OP_NOOP);
            ss |= selector_set_interest(key->s, ATTACHMENT(key)->client_fd, OP_READ);
            ret = SELECTOR_SUCCESS == ss ? REQUEST : ERROR;
        }
    } else {
        ret = ERROR;
    }

    if(ret == ERROR) {
        print_error_message("error reading CAPA response from origin");
    }
    return ret;
}

static void
capa_close(const unsigned state, struct selector_key *key) {
    
}

////////////////////////////////////////////////////////////////////////////////
// REQUEST
////////////////////////////////////////////////////////////////////////////////

static ssize_t send_next_request(struct selector_key *key, buffer * b);

/** inicializa las variables del estado REQUEST */
static void
request_init(const unsigned state, struct selector_key *key) 
{
    struct request_st * d = &ATTACHMENT(key)->client.request;
    d->reqb = &ATTACHMENT(key)->request_buffer;
    d->auxb = &ATTACHMENT(key)->aux_request_buffer;
    d->cmdb = &ATTACHMENT(key)->cmd_request_buffer;
}

/** Lee la request del cliente */
static unsigned
request_read(struct selector_key *key) 
{
    struct request_st *d = &ATTACHMENT(key)->client.request;
    enum pop3filter_state ret  = REQUEST;

    buffer *b            = d->reqb;
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

    if(ret == ERROR) {
        print_error_message_with_client_ip(ATTACHMENT(key)->client_addr, "error reading client response");
    }
    return ret;
}

/** Escribe la request en el server */
static unsigned
request_write(struct selector_key *key) 
{
    struct request_st *d = &ATTACHMENT(key)->client.request;
    enum pop3filter_state ret      = REQUEST;

    buffer *b          = d->reqb;
    ssize_t  n;

    n = send_next_request(key, b);

    if(n == -1) {
        ret = ERROR;
    } else if (n == 0) {
        selector_status ss = SELECTOR_SUCCESS;
        ss |= selector_set_interest_key(key, OP_NOOP);
        ss |= selector_set_interest(key->s, ATTACHMENT(key)->client_fd, OP_READ);
        ret = SELECTOR_SUCCESS == ss ? REQUEST : ERROR;
    }
    else {
        if (ATTACHMENT(key)->origin_pipelining) {
            if (buffer_can_read(b)) {
                if(SELECTOR_SUCCESS == selector_set_interest_key(key, OP_WRITE)) {
                    ret = REQUEST;
                } else {
                    ret = ERROR;
                }
            }
            else {
                buffer *ab            = d->auxb;
                uint8_t *aptr;
                size_t  count;

                aptr = buffer_read_ptr(ab, &count);
                n = send(ATTACHMENT(key)->origin_fd, aptr, count, MSG_NOSIGNAL);
                buffer_read_adv(ab, n);

                struct next_request *aux = ATTACHMENT(key)->client.request.next_cmd_type;
                ATTACHMENT(key)->client.request.cmd_type = aux->cmd_type;
                ATTACHMENT(key)->client.request.next_cmd_type = aux->next;
                free(aux);
                if(SELECTOR_SUCCESS == selector_set_interest_key(key, OP_READ)) {
                    ret = RESPONSE;
                } else {
                    ret = ERROR;
                }
            }
        }
        else {
            if(SELECTOR_SUCCESS == selector_set_interest_key(key, OP_READ)) {
                ret = RESPONSE;
            } else {
                ret = ERROR;
            }
        }
    }

    if(ret == ERROR) {
        print_error_message_with_client_ip(ATTACHMENT(key)->client_addr, "error writing client request to origin server");
    }

    return ret;
}

static ssize_t
send_next_request(struct selector_key *key, buffer * b) {
    buffer *cb            = ATTACHMENT(key)->client.request.cmdb;
    uint8_t *cptr;

    size_t count;
    ssize_t n = 0;

    size_t i = 0;

    if (!buffer_can_read(b))
        return 0;

    while (buffer_can_read(b) && n == 0) {
        i++;
        char c = buffer_read(b);
        if (c == '\n') {
            n = i;
        }
        buffer_write(cb, c);
    }

    if (n == 0) {
        return 0;
    }

    cptr = buffer_read_ptr(cb, &count);

    int cmd_n;
    char cmd[16], arg1[32], arg2[32], extra[5];
    char * aux = malloc(count+1);
    memcpy(aux, cptr, count);
    aux[count] = '\0';

    cmd_n = sscanf(aux, "%s %s %s %s", cmd, arg1, arg2, extra);

    if (strcasecmp(cmd, "RETR") == 0) {
        ATTACHMENT(key)->client.request.cmd_type = MULTI_RETR;
        ATTACHMENT(key)->retr_commands++;
    }   
    else if (strcasecmp(cmd, "LIST") == 0 && cmd_n == 1) {
        ATTACHMENT(key)->client.request.cmd_type = MULTI_LIST;
    }
    else if (strcasecmp(cmd, "CAPA") == 0) {
        ATTACHMENT(key)->client.request.cmd_type = MULTI_CAPA;
    }
    else if (strcasecmp(cmd, "UIDL") == 0 && cmd_n == 1) {
        ATTACHMENT(key)->client.request.cmd_type = MULTI_UIDL;
    }
    else if (strcasecmp(cmd, "TOP") == 0) {
        ATTACHMENT(key)->client.request.cmd_type = MULTI_TOP;
        ATTACHMENT(key)->top_commands++;
    }
    else if (strcasecmp(cmd, "USER") == 0) {
        ATTACHMENT(key)->client.request.cmd_type = USER;
        ATTACHMENT(key)->logged_in_username = malloc(strlen(arg1)+1);
        strcpy(ATTACHMENT(key)->logged_in_username, arg1);
    }
    else if (strcasecmp(cmd, "PASS") == 0) {
        ATTACHMENT(key)->client.request.cmd_type = PASS;
        print_login_info_message(ATTACHMENT(key)->client_addr, ATTACHMENT(key)->logged_in_username);
    }
    else if (strcasecmp(cmd, "DELE") == 0) {
        ATTACHMENT(key)->client.request.cmd_type = DELE;
        ATTACHMENT(key)->dele_commands++;
    }   
    else if (strcasecmp(cmd, "QUIT") == 0) {
        ATTACHMENT(key)->client.request.cmd_type = QUIT;
        print_logout_info_message(ATTACHMENT(key)->client_addr, ATTACHMENT(key)->logged_in_username, 
        ATTACHMENT(key)->top_commands, ATTACHMENT(key)->retr_commands, ATTACHMENT(key)->dele_commands);
    }
    else {
        ATTACHMENT(key)->client.request.cmd_type = DEFAULT;
    }

    free(aux);

    if (ATTACHMENT(key)->origin_pipelining) {
        struct next_request *next = malloc(sizeof(next));
        next->cmd_type = ATTACHMENT(key)->client.request.cmd_type;
        next->next = NULL;
        if (ATTACHMENT(key)->client.request.last_cmd_type == NULL) {
            ATTACHMENT(key)->client.request.next_cmd_type = next;
            ATTACHMENT(key)->client.request.last_cmd_type = next;
        }
        else {
            ATTACHMENT(key)->client.request.last_cmd_type->next = next;
            ATTACHMENT(key)->client.request.last_cmd_type = next;
        }

        buffer *ab            = ATTACHMENT(key)->client.request.auxb;
        uint8_t *aptr;
        size_t acount;

        aptr = buffer_write_ptr(ab, &acount);
        memcpy(aptr, cptr, count);
        buffer_write_adv(ab, count);
    }
    else {
        n = send(ATTACHMENT(key)->origin_fd, cptr, count, MSG_NOSIGNAL);
    }

    buffer_reset(cb);

    return n;  
}

static void
request_close(const unsigned state, struct selector_key *key) 
{

}

////////////////////////////////////////////////////////////////////////////////
// RESPONSE
////////////////////////////////////////////////////////////////////////////////

static pid_t start_external_process(struct selector_key *key);
static char * mallocEnv(char* s1, char* s2);
static void finish_external_process(struct selector_key *key);
static bool is_multi_response(enum request_cmd_type cmd);
static ssize_t send_next_response_status_line(struct selector_key *key, buffer * b);
static ssize_t send_next_response_multi_line(struct selector_key *key, buffer * b);

/** inicializa las variables del estado RESPONSE */
static void
response_init(const unsigned state, struct selector_key *key) 
{
    struct response_st * d = &ATTACHMENT(key)->orig.response;
    d->resb = &ATTACHMENT(key)->response_buffer;
    d->filb = &ATTACHMENT(key)->filter_buffer;
    d->stab = &ATTACHMENT(key)->status_response_buffer;
}

static pid_t
start_external_process(struct selector_key *key) {
    pid_t pid = -1;
    if (pipe(ATTACHMENT(key)->filter_in_fds) == -1 ||
        pipe(ATTACHMENT(key)->filter_out_fds) == -1) {
        return -1;
    }

    if (selector_fd_set_nio(ATTACHMENT(key)->filter_in_fds[1]) == -1 ||
        selector_fd_set_nio(ATTACHMENT(key)->filter_out_fds[0]) == -1) {
        return -1;
    }


    ATTACHMENT(key)->env_variables.filterMedias = mallocEnv("FILTER_MEDIAS=", options->censoredMediaTypes);
    ATTACHMENT(key)->env_variables.filterMessage = mallocEnv("FILTER_MSG=", options->replacementMessage);
    ATTACHMENT(key)->env_variables.pop3filter_version = mallocEnv("POP3FILTER_VERSION=", options->version);
    ATTACHMENT(key)->env_variables.pop3_username = mallocEnv("POP3_USERNAME=", ATTACHMENT(key)->logged_in_username);
    ATTACHMENT(key)->env_variables.pop3_server = mallocEnv("POP3_SERVER=", options->originServer);

    pid = fork();
    if (pid == -1) {
        return -1;
    }
    else if (pid == 0) {
        close(ATTACHMENT(key)->filter_in_fds[1]);
        close(ATTACHMENT(key)->filter_out_fds[0]);

        dup2(ATTACHMENT(key)->filter_in_fds[0], STDIN_FILENO);
        dup2(ATTACHMENT(key)->filter_out_fds[1], STDOUT_FILENO);

        putenv(ATTACHMENT(key)->env_variables.filterMedias);
        putenv(ATTACHMENT(key)->env_variables.filterMessage);
        putenv(ATTACHMENT(key)->env_variables.pop3filter_version);
        putenv(ATTACHMENT(key)->env_variables.pop3_username);
        putenv(ATTACHMENT(key)->env_variables.pop3_server);
        
        return execl("/bin/sh", "sh", "-c", options->command, NULL);
    }

    close(ATTACHMENT(key)->filter_in_fds[0]);
    close(ATTACHMENT(key)->filter_out_fds[1]);
    
    return pid;
}

static char * 
mallocEnv(char* s1, char* s2) {
    char* aux = calloc(strlen(s1) + strlen(s2) + 1, sizeof(char));
    strcat(aux, s1);
    strcat(aux, s2);
    return aux;
}

static void
finish_external_process(struct selector_key *key) {
    close(ATTACHMENT(key)->filter_in_fds[1]);
    close(ATTACHMENT(key)->filter_out_fds[0]);
    selector_unregister_fd(key->s, ATTACHMENT(key)->filter_in_fds[1]);
    selector_unregister_fd(key->s, ATTACHMENT(key)->filter_out_fds[0]);
    ATTACHMENT(key)->filter_in_fds[0] = -1;
    ATTACHMENT(key)->filter_in_fds[1] = -1;
    ATTACHMENT(key)->filter_out_fds[0] = -1;
    ATTACHMENT(key)->filter_out_fds[1] = -1;
    waitpid(ATTACHMENT(key)->filter_pid, NULL, 0);
    ATTACHMENT(key)->filter_pid = -1;

    free(ATTACHMENT(key)->env_variables.filterMedias);
    free(ATTACHMENT(key)->env_variables.filterMessage);
    free(ATTACHMENT(key)->env_variables.pop3filter_version);
    free(ATTACHMENT(key)->env_variables.pop3_username);
    free(ATTACHMENT(key)->env_variables.pop3_server);

    metrics->mailsFiltered++;
}

static bool is_multi_response(enum request_cmd_type cmd) {
    switch (cmd) {
        case MULTI_RETR:
        case MULTI_LIST:
        case MULTI_CAPA:
        case MULTI_UIDL:
        case MULTI_TOP: 
            return true;
        default:
            return false;
    }
} 

/** Lee la respuesta del origin server */
static unsigned
response_read(struct selector_key *key) 
{
    struct response_st *d = &ATTACHMENT(key)->orig.response;
    enum pop3filter_state ret      = RESPONSE;

    buffer  *b         = d->resb;
    uint8_t *ptr;
    size_t  count;
    ssize_t  n;

    ptr = buffer_write_ptr(b, &count);
    //printf("%d\n", (int)count);
    n = recv(key->fd, ptr, count, 0);
    //printf("%d\n", (int)n);

    if(n > 0) {
        buffer_write_adv(b, n);

        if (ATTACHMENT(key)->client.request.cmd_type == MULTI_RETR &&
            ATTACHMENT(key)->filter_pid != -1) {
            selector_status ss = SELECTOR_SUCCESS;
            ss |= selector_set_interest_key(key, OP_NOOP);
            ss |= selector_set_interest(key->s, ATTACHMENT(key)->filter_in_fds[1], OP_WRITE);
            ret = ss == SELECTOR_SUCCESS ? FILTER : ERROR;
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

    if(ret == ERROR) {
        print_error_message_with_client_ip(ATTACHMENT(key)->client_addr, "error reading response from origin server");
    }

    return ret;
}

/** Escribe la respuesta en el cliente */
static unsigned
response_write(struct selector_key *key) 
{
    struct response_st *d = &ATTACHMENT(key)->orig.response;
    enum pop3filter_state ret = RESPONSE;

    buffer  *b = d->resb;
    ssize_t  n;

    if (ATTACHMENT(key)->orig.response.multi_parser == NULL) {
        n = send_next_response_status_line(key, b);

        if (n == -1) {
            ret = ERROR;
        } else if (n == 0) {
            selector_status ss = SELECTOR_SUCCESS;
            ss |= selector_set_interest_key(key, OP_NOOP);
            ss |= selector_set_interest(key->s, ATTACHMENT(key)->origin_fd, OP_READ);
            ret = SELECTOR_SUCCESS == ss ? RESPONSE : ERROR;
        }
        else {
            if (is_multi_response(ATTACHMENT(key)->client.request.cmd_type)) {
                ATTACHMENT(key)->orig.response.multi_parser = parser_init(parser_no_classes(), pop3_multi_parser());
                if (ATTACHMENT(key)->client.request.cmd_type == MULTI_RETR) {
                    ATTACHMENT(key)->filter_pid = start_external_process(key);
                    ATTACHMENT(key)->filter.filter.multi_parser = parser_init(parser_no_classes(), pop3_multi_parser());
                    if (ATTACHMENT(key)->filter_pid == -1) {
                        ret = ERROR;
                    }
                    else {
                        selector_status ss = SELECTOR_SUCCESS;
                        ss |= selector_set_interest_key(key, OP_NOOP);
                        ss |= selector_register(key->s, ATTACHMENT(key)->filter_in_fds[1], &pop3filter_handler,
                                    OP_NOOP, key->data);
                        ss |= selector_register(key->s, ATTACHMENT(key)->filter_out_fds[0], &pop3filter_handler,
                                    OP_NOOP, key->data);
                        if (buffer_can_read(b)) {
                            ss |= selector_set_interest(key->s, ATTACHMENT(key)->filter_in_fds[1], OP_WRITE);
                            ret = ss == SELECTOR_SUCCESS ? FILTER : ERROR;
                        }
                        else {
                            ss |= selector_set_interest(key->s, ATTACHMENT(key)->origin_fd, OP_READ);
                            ret = ss == SELECTOR_SUCCESS ? RESPONSE : ERROR;
                        }
                    }
                }
                else {
                    selector_status ss = SELECTOR_SUCCESS;
                    ss |= selector_set_interest_key(key, OP_NOOP);
                    ss |= selector_set_interest(key->s, ATTACHMENT(key)->client_fd, OP_WRITE);
                    ret = ss == SELECTOR_SUCCESS ? RESPONSE : ERROR;
                }
            }
            else {
                if (ATTACHMENT(key)->client.request.cmd_type == QUIT) { 
                    while(ATTACHMENT(key)->client.request.next_cmd_type != NULL) {
                        struct next_request *aux = ATTACHMENT(key)->client.request.next_cmd_type;
                        ATTACHMENT(key)->client.request.next_cmd_type = aux->next;
                        free(aux);
                    }
                    ret = DONE;
                }
                else {
                    if (ATTACHMENT(key)->origin_pipelining) {
                        struct next_request *aux = ATTACHMENT(key)->client.request.next_cmd_type;
                        selector_status ss = SELECTOR_SUCCESS;
                        ss |= selector_set_interest_key(key, OP_NOOP);
                        if (aux != NULL) {
                            ATTACHMENT(key)->client.request.cmd_type = aux->cmd_type;
                            ATTACHMENT(key)->client.request.next_cmd_type = aux->next;
                            free(aux);
                            ss |= selector_set_interest(key->s, ATTACHMENT(key)->client_fd, OP_WRITE);
                            ret = ss == SELECTOR_SUCCESS ? RESPONSE : ERROR;
                        }
                        else {
                            ATTACHMENT(key)->client.request.last_cmd_type = NULL;
                            ss |= selector_set_interest(key->s, ATTACHMENT(key)->origin_fd, OP_WRITE);
                            ret = SELECTOR_SUCCESS == ss ? REQUEST : ERROR;
                        }
                    }
                    else {
                        selector_status ss = SELECTOR_SUCCESS;
                        ss |= selector_set_interest_key(key, OP_NOOP);
                        if (buffer_can_read(b)) {
                            ss |= selector_set_interest(key->s, ATTACHMENT(key)->client_fd, OP_WRITE);
                            ret = ss == SELECTOR_SUCCESS ? RESPONSE : ERROR;
                        }
                        else {
                            ss |= selector_set_interest(key->s, ATTACHMENT(key)->origin_fd, OP_WRITE);
                            ret = SELECTOR_SUCCESS == ss ? REQUEST : ERROR;
                        }
                    }
                }
            }
        }
    }
    else {
        if (ATTACHMENT(key)->filter_pid != -1) {
            b = d->filb;
        }

        n = send_next_response_multi_line(key, b);

        if(n == -1) {
            ret = ERROR;
        } else if (n == 0) {
            if (ATTACHMENT(key)->client.request.cmd_type == MULTI_RETR) {
                selector_status ss = SELECTOR_SUCCESS;
                ss |= selector_set_interest_key(key, OP_NOOP);
                ss |= selector_set_interest(key->s, ATTACHMENT(key)->filter_out_fds[0], OP_READ);
                ret = ss == SELECTOR_SUCCESS ? FILTER : ERROR;
            }
            else {
                selector_status ss = SELECTOR_SUCCESS;
                ss |= selector_set_interest_key(key, OP_NOOP);
                ss |= selector_set_interest(key->s, ATTACHMENT(key)->origin_fd, OP_READ);
                ret = SELECTOR_SUCCESS == ss ? RESPONSE : ERROR;
            }
        } 
        else {
            if (ATTACHMENT(key)->origin_pipelining) {
                struct next_request *aux = ATTACHMENT(key)->client.request.next_cmd_type;
                selector_status ss = SELECTOR_SUCCESS;
                ss |= selector_set_interest_key(key, OP_NOOP);
                if (aux != NULL) {
                    ATTACHMENT(key)->client.request.cmd_type = aux->cmd_type;
                    ATTACHMENT(key)->client.request.next_cmd_type = aux->next;
                    free(aux);
                    ss |= selector_set_interest(key->s, ATTACHMENT(key)->client_fd, OP_WRITE);
                    ret = ss == SELECTOR_SUCCESS ? RESPONSE : ERROR;
                }
                else {
                    ATTACHMENT(key)->client.request.last_cmd_type = NULL;
                    ss |= selector_set_interest(key->s, ATTACHMENT(key)->origin_fd, OP_WRITE);
                    ret = SELECTOR_SUCCESS == ss ? REQUEST : ERROR;
                }
            }
            else {
                selector_status ss = SELECTOR_SUCCESS;
                ss |= selector_set_interest_key(key, OP_NOOP);
                if (buffer_can_read(b)) {
                    ss |= selector_set_interest(key->s, ATTACHMENT(key)->client_fd, OP_WRITE);
                    ret = ss == SELECTOR_SUCCESS ? RESPONSE : ERROR;
                }
                else {
                    ss |= selector_set_interest(key->s, ATTACHMENT(key)->origin_fd, OP_WRITE);
                    ret = SELECTOR_SUCCESS == ss ? REQUEST : ERROR;
                }
            }
        }
    }

    if(ret == ERROR) {
        print_error_message_with_client_ip(ATTACHMENT(key)->client_addr, "error writing origin server response to client");
    }

    return ret;
}

static ssize_t
send_next_response_status_line(struct selector_key *key, buffer * b) {
    buffer *sb            = ATTACHMENT(key)->orig.response.stab;
    uint8_t *sptr;

    size_t count;
    ssize_t n = 0;

    size_t i = 0;

    while (buffer_can_read(b) && n == 0) {
        i++;
        char c = buffer_read(b);
        if (c == '\n') {
            n = i;
        }
        buffer_write(sb, c);
    }

    if (n == 0) {
        return 0;
    }

    sptr = buffer_read_ptr(sb, &count);

    if (strncasecmp((char*)sptr, "+OK", 3) != 0) {
        ATTACHMENT(key)->client.request.cmd_type = DEFAULT;
    }

    n = send(ATTACHMENT(key)->client_fd, sptr, count, MSG_NOSIGNAL);

    buffer_reset(sb);

    return n;
}

static ssize_t
send_next_response_multi_line(struct selector_key *key, buffer * b) {
    uint8_t *ptr;
    size_t i = 0;
    size_t  count;
    ssize_t  n;

    if (!buffer_can_read(b))
        return 0;

    ptr = buffer_read_ptr(b, &count);

    while (buffer_can_read(b)) {
        i++;
        uint8_t c = buffer_read(b);
        const struct parser_event *e = parser_feed(ATTACHMENT(key)->orig.response.multi_parser, c);
        if (e->type == POP3_MULTI_FIN) {
            if (ATTACHMENT(key)->filter_pid != -1) {
                finish_external_process(key);
            }
            parser_destroy(ATTACHMENT(key)->orig.response.multi_parser);
            ATTACHMENT(key)->orig.response.multi_parser = NULL;
            n = send(key->fd, ptr, i, MSG_NOSIGNAL);
            return n;
        }
    }

    n = send(key->fd, ptr, count, MSG_NOSIGNAL);
    return 0;
}

static void
response_close(const unsigned state, struct selector_key *key) 
{

}

////////////////////////////////////////////////////////////////////////////////
// FILTER
////////////////////////////////////////////////////////////////////////////////

static ssize_t
write_next_response_multi_line_to_filter(struct selector_key *key, buffer * b);

/** inicializa las variables del estado FILTER */
static void
filter_init(const unsigned state, struct selector_key *key) 
{
    struct filter_st * d = &ATTACHMENT(key)->filter.filter;
    d->resb = &ATTACHMENT(key)->response_buffer;
    d->filb = &ATTACHMENT(key)->filter_buffer;
}

/** Lee el mail filtrado del stdout del filter */
static unsigned
filter_read(struct selector_key *key) 
{
    struct filter_st *d = &ATTACHMENT(key)->filter.filter;
    enum pop3filter_state ret      = FILTER;

    buffer  *b         = d->filb;
    uint8_t *ptr;
    size_t  count;
    ssize_t  n;

    ptr = buffer_write_ptr(b, &count);
    n = read(key->fd, ptr, count);
    //printf("leyendo del filter\n");

    if(n > 0) {
        buffer_write_adv(b, n);
        selector_status ss = SELECTOR_SUCCESS;
        ss |= selector_set_interest_key(key, OP_NOOP);
        ss |= selector_set_interest(key->s, ATTACHMENT(key)->client_fd, OP_WRITE);
        ret = ss == SELECTOR_SUCCESS ? RESPONSE : ERROR;
    } else {
        ret = ERROR;
    }

    if(ret == ERROR) {
        print_error_message_with_client_ip(ATTACHMENT(key)->client_addr, "error reading mail filter output");
    }

    return ret;
}

/** Escribe el mail en el stdin del filter */
static unsigned
filter_write(struct selector_key *key) 
{
    struct filter_st *d = &ATTACHMENT(key)->filter.filter;
    enum pop3filter_state ret = FILTER;

    buffer  *b         = d->resb;
    ssize_t  n;

    n = write_next_response_multi_line_to_filter(key, b);
    //printf("escribiendo en el filter\n");

    if(n == -1) {
        ret = ERROR;
    } 
    else if (n == 0) {
        selector_status ss = SELECTOR_SUCCESS;
        ss |= selector_set_interest_key(key, OP_NOOP);
        ss |= selector_set_interest(key->s, ATTACHMENT(key)->origin_fd, OP_READ);
        ret = SELECTOR_SUCCESS == ss ? RESPONSE : ERROR;
    } else {
        selector_status ss = SELECTOR_SUCCESS;
        ss |= selector_set_interest_key(key, OP_NOOP);
        ss |= selector_set_interest(key->s, ATTACHMENT(key)->filter_out_fds[0], OP_READ);
        ret = ss == SELECTOR_SUCCESS ? FILTER : ERROR;
    }

    if(ret == ERROR) {
        print_error_message_with_client_ip(ATTACHMENT(key)->client_addr, "error writing mail filter output");
    }
    
    return ret;
}

static ssize_t
write_next_response_multi_line_to_filter(struct selector_key *key, buffer * b) {
    uint8_t *ptr;
    size_t i = 0;
    size_t  count;
    ssize_t  n;

    if (!buffer_can_read(b))
        return 0;

    ptr = buffer_read_ptr(b, &count);

    while (buffer_can_read(b)) {
        i++;
        uint8_t c = buffer_read(b);
        const struct parser_event *e = parser_feed(ATTACHMENT(key)->filter.filter.multi_parser, c);
        if (e->type == POP3_MULTI_FIN) {
            parser_destroy(ATTACHMENT(key)->filter.filter.multi_parser);
            ATTACHMENT(key)->filter.filter.multi_parser = NULL;
            n = write(key->fd, ptr, i);
            metrics->bytesTransferred += n;
            return n;
        }
    }

    n = write(key->fd, ptr, count);
    metrics->bytesTransferred += n;
    return 0;
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
        .state            = CAPA,
        .on_arrival       = capa_init,
        .on_read_ready    = capa_read,
        .on_departure     = capa_close,
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
    struct state_machine *stm   = &ATTACHMENT(key)->stm;
    const enum pop3filter_state st    = (enum pop3filter_state)stm_state(stm);

    if(ERROR == st || DONE == st) {
        pop3filter_destroy(ATTACHMENT(key));
        metrics->concurrentConnections--;
    }
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

    metrics->concurrentConnections--;
}