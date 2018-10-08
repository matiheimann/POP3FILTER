#include <unistd.h>
#include <stdint.h>
#include <stdio.h> 
#include <sys/socket.h> 
#include <stdlib.h>
#include <netinet/in.h> 
#include <string.h>
#include <ctype.h> 
#include "include/pop3Filter.h"


static char* errorFile = "/dev/null";
static uint32_t pop3Direction = INADDR_ANY;
static uint32_t managmentDirection = INADDR_LOOPBACK;
static char* replacementMessage = "Parte reemplazada";
static int selectedReplacementMessage = 0;
static char** censurableMediaTypes = NULL;
static int censurableMediaTypesSize = 0;
static int managmentPort = 9090;
static int localPort = 1110;
static int originPort = 110;
static char* command = "";
char* version = "1.0";

int main(int argc, char* const argv[])
{
	setConfiguration(argc, argv);
	return 0;
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
				addCensurableMediaType(optarg);
				break;
			case 'o':
				setManagmentPort(optarg);
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
	printf("-M to add a censurable media type\n");
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


void addCensurableMediaType(char* mediaType)
{
	censurableMediaTypes = realloc(censurableMediaTypes, (censurableMediaTypesSize + 1) * sizeof(char*));
	censurableMediaTypes[censurableMediaTypesSize] = mediaType;
	censurableMediaTypesSize++;
}

void setManagmentPort(char* port)
{
	isANumericArgument(port, 'o');
	managmentPort = atoi(port);
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