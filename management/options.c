#include <stdio.h>
#include <ctype.h>
#include <stdlib.h>
#include <stdint.h>
#include <string.h>
#include <unistd.h>
#include <netinet/in.h>
#include <sys/socket.h> 

#include "options.h"

options_st * options;

void setConfiguration(int argc, char* const argv[])
{
	/* Initialize default values */
   	options = malloc(sizeof(*options));
	options->pop3Direction = INADDR_ANY;
	options->managementDirection = INADDR_LOOPBACK;
	options->managementPort = 9090;

	char c;

	while((c = getopt(argc, argv, "L:o")) != -1)
	{
		switch(c)
		{
			case 'L':
				setManagementDirection(optarg);
				break;
			case 'o':
				setManagementPort(optarg);
				break;
			default:
				exit(0);
				break;
		}
	}
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

void setManagementDirection(char* dir)
{
	isANumericArgument(dir, 'L');
	options->managementDirection = atoi(dir);
}

void setManagementPort(char* port)
{
	isANumericArgument(port, 'o');
	options->managementPort = atoi(port);
}
