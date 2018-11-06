#include <unistd.h>
#include <stdint.h>
#include <stdio.h> 
#include <errno.h>
#include <signal.h>
#include <sys/socket.h> 
#include <sys/types.h>
#include <sys/stat.h>
#include <stdlib.h>
#include <netinet/in.h>
#include <netdb.h>
#include <string.h>
#include <ctype.h>

#include "options.h"
#include "metrics.h"
#include "iputils.h"
#include "mediatypes.h"

options_st * options;

void setConfiguration(int argc, char* const argv[])
{
	/* Initialize default values */
   	options = malloc(sizeof(*options));
	options->errorFile = "/dev/null";
	options->replacementMessage = (char*) calloc(250, sizeof(char));
	strcpy(options->replacementMessage,"Parte reemplazada");
	options->selectedReplacementMessage = 0;
	options->censoredMediaTypes = (char*) calloc(250, sizeof(char));
	strcpy(options->censoredMediaTypes,"");
	options->managementPort = 9090;
	options->localPort = 1110;
	options->originPort = 110;
	options->command = (char*) calloc(250, sizeof(char));
	strcpy(options->command, "cat");
	options->version = "1.0";

	memset(&(options->pop3Address), 0, sizeof(options->pop3Address));
	memset(&(options->managementAddress), 0, sizeof(options->managementAddress));

	// IPv4 and IPv6
	/*
	options->pop3Address.sin6_family = AF_INET6;
	options->pop3Address.sin6_addr = in6addr_any;
	options->managementAddress.sin6_family = AF_INET6;
	options->managementAddress.sin6_addr = in6addr_loopback;
	*/

	// Only IPv4
	((struct sockaddr_in*)&options->pop3Address)->sin_family = AF_INET;
	((struct sockaddr_in*)&options->pop3Address)->sin_addr.s_addr = htonl(INADDR_ANY);
	((struct sockaddr_in*)&options->managementAddress)->sin_family = AF_INET;
	((struct sockaddr_in*)&options->managementAddress)->sin_addr.s_addr = htonl(INADDR_LOOPBACK);

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
				setPop3Address(optarg);
				break;
			case 'L':
				setManagementAddress(optarg);
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

	if (argv[optind] == NULL) 
  		printHelp();
	else
		setOriginServer(argv[optind]);
}

void setOriginServer(char* server)
{
	options->originServer = server;
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

void setPop3Address(char* dir)
{
	options->pop3Address.sin6_family = string_to_ip(dir, (struct sockaddr*)&(options->pop3Address));

}

void setManagementAddress(char* dir)
{
	options->managementAddress.sin6_family = string_to_ip(dir, (struct sockaddr*)&(options->managementAddress));
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

void setReplacementMessage(char* message)
{
	if(options->selectedReplacementMessage == 0)
	{
		strcpy(options->replacementMessage, message);
		options->selectedReplacementMessage = 1;
		return;
	}
	char* aux1 = options->replacementMessage;
	options->replacementMessage = strcatFixStrings(options->replacementMessage, "\n");
	char* aux2 = options->replacementMessage;
	options->replacementMessage = strcatFixStrings(options->replacementMessage, message);
	free(aux1);
	free(aux2);
}

void addCensoredMediaType(char* mediaType)
{
	if(!isValidMediaType(mediaType))
	{
		printf("%s is not a valid media type\n", mediaType);
		return;
	}
	if(strcmp(options->censoredMediaTypes, "") == 0)
	{
		options->censoredMediaTypes = mediaType;
		return;
	}
	options->censoredMediaTypes = strcatFixStrings(options->censoredMediaTypes, ",");
	options->censoredMediaTypes = strcatFixStrings(options->censoredMediaTypes, mediaType);
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
	/*if(isValidFile(cmd))
	{*/
	strcpy(options->command, cmd);
	/*}
	else
	{
		printf("%s is not a valid file to use as command\n", cmd);
		exit(1);
	}*/
	
}

void printVersion()
{
	printf("The version is: %s\n", options->version);
	exit(0);
}

char * strcatFixStrings(char* s1, char* s2)
{
	char* aux = calloc((strlen(s1) + strlen(s2) + 1), sizeof(char));
	strcat(aux, s1);
	strcat(aux, s2);
	printf("%s\n", aux);
	return aux;
}

int isValidFile(char* file)
{
	struct stat fileStat;
	stat(file, &fileStat);
	//Check if the file passed as argument is a Directory or a non executable file
	return (S_ISREG(fileStat.st_mode) && access(file, X_OK) != -1) ? 1 : 0;
}
