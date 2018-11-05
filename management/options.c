#include <stdio.h>
#include <ctype.h>
#include <stdlib.h>
#include <stdint.h>
#include <string.h>
#include <unistd.h>
#include <netinet/in.h>
#include <sys/socket.h> 

#include "options.h"
#include "iputils.h"

options_st * options;

void setConfiguration(int argc, char* const argv[])
{
	/* Initialize default values */
   	options = malloc(sizeof(*options));
	memset(&(options->managementAddress), 0, sizeof(options->managementAddress));
	((struct sockaddr_in*)&options->managementAddress)->sin_family = AF_INET;
	((struct sockaddr_in*)&options->managementAddress)->sin_addr.s_addr = htonl(INADDR_LOOPBACK);
	options->managementPort = 9090;

	char c;

	while((c = getopt(argc, argv, "L:o")) != -1)
	{
		switch(c)
		{
			case 'L':
				setManagementAddress(optarg);
				break;
			case 'o':
				setManagementPort(optarg);
				break;
			case 'h':
				printHelp();
				break;
			default:
				exit(0);
				break;
		}
	}
	if (argv[optind] == NULL) 
  		printHelp();
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

void setManagementAddress(char* dir)
{
	options->managementAddress.sin6_family = string_to_ip(dir, (struct sockaddr*)&(options->managementAddress));
}


void setManagementPort(char* port)
{
	isANumericArgument(port, 'o');
	options->managementPort = atoi(port);
}

void printHelp()
{
	printf("Use: management [OPTIONS] <proxy>\n");
    printf(" - Management tool for changing pop3filter configuration\n");
    printf("and geting metrics -\n\n");
    printf("Options:\n");
	printf("-h for help.\n");
	printf("-L to set the management direction of the pop3filter.\n");
	printf("-o to set the management port of the pop3filter.\n");
	exit(0);
}