#include <unistd.h>
#include <stdint.h>
#include <stdio.h> 
#include <errno.h>
#include <signal.h>
#include <sys/socket.h> 
#include <stdlib.h>
#include <netinet/in.h>
#include <netinet/sctp.h> 
#include <string.h>
#include <ctype.h> 

#include "selector.h"
#include "options.h"
#include "metrics.h"
#include "pop3filter.h"
#include "management.h"

static bool done = false;

static void
sigterm_handler(const int signal) {
    printf("signal %d, cleaning up and exiting\n", signal);
    done = true;
}

int main(int argc, char* const argv[])
{
	setConfiguration(argc, argv);
    setMetrics();	

	// no tenemos nada que leer de stdin
    close(0);
	const char       *err_msg = NULL;
    selector_status   ss      = SELECTOR_SUCCESS;
    fd_selector selector      = NULL;

    if (((struct sockaddr*)&options->pop3Address)->sa_family == AF_INET)
        ((struct sockaddr_in*)&options->pop3Address)->sin_port = htons(options->localPort);
    else if (((struct sockaddr*)&options->pop3Address)->sa_family == AF_INET6)
        ((struct sockaddr_in6*)&options->pop3Address)->sin6_port = htons(options->localPort);

    if (((struct sockaddr*)&options->managementAddress)->sa_family == AF_INET)
        ((struct sockaddr_in*)&options->managementAddress)->sin_port = htons(options->managementPort);
    else if (((struct sockaddr*)&options->managementAddress)->sa_family == AF_INET6)
        ((struct sockaddr_in6*)&options->managementAddress)->sin6_port = htons(options->managementPort);

    const int server = socket(((struct sockaddr*)&options->pop3Address)->sa_family, SOCK_STREAM, IPPROTO_TCP);
    const int managementServer = socket(((struct sockaddr*)&options->managementAddress)->sa_family, SOCK_STREAM, IPPROTO_SCTP);
    if(server < 0 || managementServer < 0) {
        err_msg = "unable to create socket";
       	goto finally;
    }

	setsockopt(server, SOL_SOCKET, SO_REUSEADDR, &(int){ 1 }, sizeof(int));
	setsockopt(managementServer, SOL_SOCKET, SO_REUSEADDR, &(int){ 1 }, sizeof(int));

	if(bind(server, (struct sockaddr*) &(options->pop3Address), sizeof(options->pop3Address)) < 0 || 
       bind(managementServer, (struct sockaddr*) &(options->managementAddress), sizeof(options->managementAddress)) < 0) {
        err_msg = "unable to bind socket";
        goto finally;
    }

    struct sctp_initmsg initmsg;
  	memset(&initmsg, 0, sizeof(initmsg));
 	initmsg.sinit_num_ostreams = 1;
  	initmsg.sinit_max_instreams = 1;
  	initmsg.sinit_max_attempts = 4;
  	setsockopt(managementServer, IPPROTO_SCTP, SCTP_INITMSG, &initmsg, sizeof(initmsg));

    if (listen(server, 20) < 0 || listen(managementServer, 5) < 0) {
        err_msg = "unable to listen";
        goto finally;
    }

    fprintf(stdout, "Listening on TCP port %d (pop3filter)\n", options->localPort);
    fprintf(stdout, "Listening on SCTP port %d (management)\n", options->managementPort);

	// registrar sigterm es útil para terminar el programa normalmente.
    // esto ayuda mucho en herramientas como valgrind.
    signal(SIGTERM, sigterm_handler);
    signal(SIGINT,  sigterm_handler);

	if(selector_fd_set_nio(server) == -1 || selector_fd_set_nio(managementServer) == -1) {
        err_msg = "getting server socket flags";
        goto finally;
    }

	const struct selector_init conf = {
        .signal = SIGALRM,
        .select_timeout = {
            .tv_sec  = 10,
            .tv_nsec = 0,
        },
    };

    if(0 != selector_init(&conf)) {
        err_msg = "initializing selector";
        goto finally;
    }

	selector = selector_new(1024);
    if(selector == NULL) {
        err_msg = "unable to create selector";
        goto finally;
    }

	const struct fd_handler pop3filter = {
        .handle_read       = pop3filter_passive_accept,
        .handle_write      = NULL,
        .handle_close      = NULL, // nada que liberar
    };

    const struct fd_handler management = {
        .handle_read       = management_passive_accept,
        .handle_write      = NULL,
        .handle_close      = NULL, // nada que liberar
    };
	
	ss = selector_register(selector, server, &pop3filter, OP_READ, NULL);
	ss = selector_register(selector, managementServer, &management, OP_READ, NULL);

    if(ss != SELECTOR_SUCCESS) {
        err_msg = "registering fd";
        goto finally;
    }

	while(!done)
	{
		err_msg = NULL;
        ss = selector_select(selector);
        if(ss != SELECTOR_SUCCESS) {
            err_msg = "serving";
            goto finally;
        }
	}

	if(err_msg == NULL) {
        err_msg = "closing";
    }

	int ret = 0;

finally:
	if(ss != SELECTOR_SUCCESS) {
        fprintf(stderr, "%s: %s\n", (err_msg == NULL) ? "": err_msg,
                                  ss == SELECTOR_IO
                                      ? strerror(errno)
                                      : selector_error(ss));
        ret = 2;
    } else if(err_msg) {
        perror(err_msg);
        ret = 1;
    }

    if(selector != NULL) {
        selector_destroy(selector);
    }

    selector_close();

	pop3filter_pool_destroy();

    if(server >= 0) {
        close(server);
    }

    if(managementServer >= 0) {
        close(managementServer);
    }

    return ret;
}