#include <unistd.h>
#include <stdint.h>
#include <stdio.h> 
#include <errno.h>
#include <signal.h>
#include <sys/socket.h> 
#include <stdlib.h>
#include <netinet/in.h>
#include <netdb.h>
#include <string.h>
#include <ctype.h> 

#include "options.h"

options_st * options;

void setConfiguration(int argc, char* const argv[])
{
	/* Initialize default values */
   	options = malloc(sizeof(*options));
	options->errorFile = "/dev/null";
	options->pop3Direction = INADDR_ANY;
	options->managementDirection = INADDR_LOOPBACK;
	options->replacementMessage = "Parte reemplazada";
	options->selectedReplacementMessage = 0;
	options->censurableMediaTypes = NULL;
	options->censurableMediaTypesSize = 0;
	options->managementPort = 9090;
	options->localPort = 1110;
	options->originPort = 110;
	options->command = "";
	options->version = "1.0";

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
				setManagementDirection(optarg);
				break;
			case 'm':
				setReplacementMessage(optarg);
				break;
			case 'M':
				addCensurableMediaType(optarg);
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

	if (argv[optind] == NULL) 
	{
  		printf("Missing origin-server argument\n");
  		exit(0);
	}
	else
		setOriginServer(argv[optind]);
}

void setOriginServer(char* server)
{
	options->originServer = server;
	resolv_address(options->originServer, options->originPort, &options->originAddrInfo);
}

void resolv_address(char *address, uint16_t port, struct addrinfo ** addrinfo) 
{
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
    snprintf(buff, sizeof(buff), "%d", port);

    printf("Resolving \"%s\" ip...\n", address);
    if (0 != getaddrinfo(address, buff, &hints, addrinfo)) {
        fprintf(stderr,"Domain name resolution error\n");
        exit(0);
    }
    printf("Domain name resolved with success\n");

}

void setErrorFile(char* file)
{
	options->errorFile = file;
}

void printHelp()
{
	printf("Uso: pop3filter [OPTIONS] <servidor-origen>\n");
    printf(" - Proxy POP3 que permite transformar los mensajes entregados -\n\n");
    printf("Opciones:\n");
	printf("-e to set error file.\n");
	printf("-h for help.\n");
	printf("-l to set POP3 direction.\n");
	printf("-L to set management direction.\n");
	printf("-m to set replacement message.\n");
	printf("-M to add a censurable media type\n");
	printf("-o to set the management port\n");
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
	options->pop3Direction = atoi(dir);
}

void setManagementDirection(char* dir)
{
	isANumericArgument(dir, 'L');
	options->managementDirection = atoi(dir);
}

void setReplacementMessage(char* message)
{
	if(options->selectedReplacementMessage == 0)
	{
		options->replacementMessage = message;
		options->selectedReplacementMessage = 1;
		return;
	}
	char* aux = malloc((strlen(options->replacementMessage) + strlen(message) + 3) * sizeof(char));
	strcat(aux, options->replacementMessage);
	strcat(aux, "\n");
	strcat(aux, message);
	options->replacementMessage = aux;

}

void addCensurableMediaType(char* mediaType)
{
	options->censurableMediaTypes = realloc(options->censurableMediaTypes, (options->censurableMediaTypesSize + 1) * sizeof(char*));
	options->censurableMediaTypes[options->censurableMediaTypesSize] = mediaType;
	options->censurableMediaTypesSize++;
}

void setManagementPort(char* port)
{
	isANumericArgument(port, 'o');
	options->managementPort = atoi(port);
}

void setLocalPort(char* port)
{
	isANumericArgument(port, 'p');
	options->localPort = atoi(port);
}

void setOriginPort(char* port)
{
	isANumericArgument(port, 'P');
	options->originPort = atoi(port);
}

void setCommand(char* cmd)
{
	options->command = cmd;
}

void printVersion()
{
	printf("The version is: %s\n", options->version);
	exit(0);
}