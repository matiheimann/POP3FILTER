#include <unistd.h>
#include <stdint.h>
#include <stdio.h> 
#include <errno.h>
#include <signal.h>
#include <sys/socket.h> 
#include <stdlib.h>
#include <netinet/in.h> 
#include <string.h>
#include <ctype.h> 

#include "selector.h"
#include "main.h"
#include "pop3filter.h"


static char* errorFile = "/dev/null";
static uint32_t pop3Direction = INADDR_ANY;
static uint32_t managmentDirection = INADDR_LOOPBACK;
static char* replacementMessage = "Parte reemplazada";
static int selectedReplacementMessage = 0;
static char** censoredMediaTypes = NULL;
static int censoredMediaTypesSize = 0;
static int managementPort = 9090;
static int localPort = 1110;
static int originPort = 110;
static char* command = "";
char* version = "1.0";

static bool done = false;

static void
sigterm_handler(const int signal) {
    printf("signal %d, cleaning up and exiting\n", signal);
    done = true;
}

int main(int argc, char* const argv[])
{
	setConfiguration(argc, argv);

	// no tenemos nada que leer de stdin
    close(0);
	const char       *err_msg = NULL;
    selector_status   ss      = SELECTOR_SUCCESS;
    fd_selector selector      = NULL;

    struct sockaddr_in addr;
    memset(&addr, 0, sizeof(addr));
    addr.sin_family      = AF_INET; // TODO investigate. 
    addr.sin_addr.s_addr = htonl(pop3Direction);
    addr.sin_port        = htons(localPort);

    const int server = socket(AF_INET, SOCK_STREAM, IPPROTO_TCP); // TODO investigate.
    if(server < 0) {
        err_msg = "unable to create socket";
       	goto finally;
    }

	fprintf(stdout, "Listening on TCP port %d\n", localPort);
	setsockopt(server, SOL_SOCKET, SO_REUSEADDR, &(int){ 1 }, sizeof(int));

	if(bind(server, (struct sockaddr*) &addr, sizeof(addr)) < 0) {
        err_msg = "unable to bind socket";
        goto finally;
    }

    if (listen(server, 20) < 0) {
        err_msg = "unable to listen";
        goto finally;
    }
	// registrar sigterm es útil para terminar el programa normalmente.
    // esto ayuda mucho en herramientas como valgrind.
    signal(SIGTERM, sigterm_handler);
    signal(SIGINT,  sigterm_handler);

	if(selector_fd_set_nio(server) == -1) {
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
	
	ss = selector_register(selector, server, &pop3filter,
                                              OP_READ, NULL);
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
    return ret;
}

void setConfiguration(int argc, char* const argv[])
{
	char c;
	while((c = getopt(argc, argv, "e:hl:L:m:M:o:p:P:t:v")) != -1)
	{
		switch(c)
		{
			case 'e':
				setErrorFile(optarg);
				break;
			case 'h':
				printHelp();
				break;
			case 'l':
				setPop3Direction(optarg);
				break;
			case 'L':
				setManagmentDirection(optarg);
				break;
			case 'm':
				setReplacementMessage(optarg);
				break;
			case 'M':
				addCensoredMediaType(optarg);
				break;
			case 'o':
				setManagementPort(optarg);
				break;
			case 'p':
				setLocalPort(optarg);
				break;
			case 'P':
				setOriginPort(optarg);
				break;
			case 't':
				setCommand(optarg);
				break;
			case 'v':
				printVersion();
				break;
			default:
				exit(0);
				break;
		}
	}
}

void setErrorFile(char* file)
{
	errorFile = file;
}

void printHelp()
{
	printf("-e to set error file.\n");
	printf("-h for help.\n");
	printf("-l to set POP3 direction.\n");
	printf("-L to set managment direction.\n");
	printf("-m to set replacement message.\n");
	printf("-M to add a censored media type\n");
	printf("-o to set the managment port\n");
	printf("-p to set the local port\n");
	printf("-P to set the origin port\n");
	printf("-t to set a command for extern transformations\n");
	printf("-v to know the POP3 Filter version\n");
	exit(0);
}

int isANumericArgument(char* value, char param)
{
	int i = 0;
	while(value[i])
	{
		if(!isdigit(value[i]))
		{
			printf("%s is not a valid argument for -%c\n", value, param);
			exit(0);
		}
		i++;
	}
	return 1;
}

void setPop3Direction(char* dir)
{
	isANumericArgument(dir, 'l');
	pop3Direction = atoi(dir);
}

void setManagmentDirection(char* dir)
{
	isANumericArgument(dir, 'L');
	managmentDirection = atoi(dir);
}

void setReplacementMessage(char* message)
{
	if(selectedReplacementMessage == 0)
	{
		replacementMessage = message;
		selectedReplacementMessage = 1;
		return;
	}
	char* aux = malloc((strlen(replacementMessage) + strlen(message) + 3) * sizeof(char));
	strcat(aux, replacementMessage);
	strcat(aux, "\n");
	strcat(aux, message);
	replacementMessage = aux;

}


void addCensoredMediaType(char* mediaType)
{
	censoredMediaTypes = realloc(censoredMediaTypes, (censoredMediaTypesSize + 1) * sizeof(char*));
	censoredMediaTypes[censoredMediaTypesSize] = mediaType;
	censoredMediaTypesSize++;
}

void setManagementPort(char* port)
{
	isANumericArgument(port, 'o');
	managementPort = atoi(port);
}

void setLocalPort(char* port)
{
	isANumericArgument(port, 'p');
	localPort = atoi(port);
}

void setOriginPort(char* port)
{
	isANumericArgument(port, 'P');
	originPort = atoi(port);
}

void setCommand(char* cmd)
{
	command = cmd;
}

void printVersion()
{
	printf("The version is: %s\n", version);
	exit(0);
}
