#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <ctype.h>

#include "managementParser.h"
#include "mediatypes.h"

#define BUFFER_COMMAND_SIZE 200

// TODO: check password max length

unsigned char* readCommand(int* size)
{
	char buffer[BUFFER_COMMAND_SIZE]  = {0};
	unsigned char* ret;
	fgets(buffer, BUFFER_COMMAND_SIZE, stdin);
	*strstr(buffer, "\n") = '\0';
	if(strcmp(buffer, "concurrent connections") == 0)
	{
		ret = (unsigned char*) calloc(2, sizeof(char));
		*ret = 0x01;
		*(ret+1) = 0x1; //version 1.0
		*size = 2;
	} 
	else if(strcmp(buffer, "historical accesses") == 0)
	{
		ret = (unsigned char*) calloc(2, sizeof(char));
		*ret = 0x2;
		*(ret+1) = 0x1;
		*size = 2;
	}
	else if(strcmp(buffer, "transfered bytes") == 0)
	{
		ret = (unsigned char*) calloc(2, sizeof(char));
		*(ret) = 0x3;
		*(ret+1) = 0x1;
		*size = 2;
	}
	else if(strcmp(buffer, "filtered mails") == 0)
	{
		ret = (unsigned char*) calloc(2, sizeof(char));
		*(ret) = 0x4;
		*(ret+1) = 0x1;
		*size = 2;
	}
	else if(strcmp(buffer, "get mediatypes") == 0)
	{
		ret = (unsigned char*) calloc(3, sizeof(char));
		*(ret) = 0x80; // 10000000
		*(ret + 1) = 0x1;
		*(ret + 2) = 0x0; 
		*size = 3;
	}
	else if(strcmp(buffer,"get replacement message") == 0)
	{
		ret = (unsigned char*) calloc(3, sizeof(char));
		*(ret) = 0x80; // 10000000
		*(ret + 1) = 0x1;
		*(ret + 2) = 0x1;
		*size = 3;
	}
	else if(strcmp(buffer, "get filter command") == 0)
	{
		ret = (unsigned char*) calloc(3, sizeof(char));
		*(ret) = 0x80; // 10000000
		*(ret + 1) = 0x1;
		*(ret + 2) = 0x2;
		*size = 3;
	}
	else if(startsWith(buffer, "set mediatypes ") && checkMediaTypes(buffer + 15))
	{
		int strlength = strlen(buffer + 15) + 1;
		ret = (unsigned char*) calloc(strlength + 3, sizeof(char));
		*(ret) = 0x81; // 10000000
		*(ret + 1) = 0x1;
		*(ret + 2) = 0x0;
		strcpy((char*) (ret + 3), buffer + 15);
		*size = strlength + 3;
	}
	else if(startsWith(buffer, "set replacement message "))
	{
		int strlength = strlen(buffer + 24);
		ret = (unsigned char*) calloc(strlength + 4, sizeof(char));
		*(ret) = 0x81;
		*(ret + 1) = 0x1;
		*(ret + 2) = 0x1;
		strcpy((char*) (ret + 3), buffer + 24);
		*size = strlength + 4;
	}
	else if(startsWith(buffer, "set filter command ")) // TODO: check valid command
	{
		int strlength = strlen(buffer + 19);
		ret = (unsigned char*) calloc(strlength + 4, sizeof(char));
		*(ret) = 0x81;
		*(ret + 1) = 0x1;
		*(ret + 2) = 0x2;
		strcpy((char*) (ret + 3), buffer + 19);
		*size = strlength + 4;
	}
	else if(startsWith(buffer, "user ")) // TODO: check valid user
	{
		int strlength = strlen(buffer + 5);
		ret = (unsigned char*) calloc(strlength + 3, sizeof(char));
		*(ret) = 0x83;
		*(ret + 1) = 0x1;
		strcpy((char*) ret + 2, buffer + 5);
		*size = strlength + 3;
	}
	else if(startsWith(buffer, "password ")) // TODO: check valid password
	{
		int strlength = strlen(buffer + 9);
		ret = (unsigned char*) calloc(strlength + 3, sizeof(char));
		*(ret) = 0x84;
		*(ret + 1) = 0x1;
		strcpy((char*) ret + 2, buffer + 9);
		*size = strlength + 3;
	}
	else if(strcmp(buffer, "help") == 0)
	{
		helpMessage();
		ret = NULL;
	}
	else if(strcmp(buffer, "exit") == 0)
	{
		ret = (unsigned char*) calloc(3, sizeof(char));
		*(ret) = 0x0;
		*(ret + 1) = 0x1;
		*size = 2;
	}
	else
	{
		printf("Command is not valid.\n");
		ret = NULL;
	}
	return ret;
}

int startsWith(const char* str, const char* start)
{
	while(*str && *start)
	{
		if(*str != *start)
			return 0;
		str++;
		start++;
	}
	if(*str == 0 && *start != 0)
		return 0;
	return 1;
}

void helpMessage()
{
	printf("These are the available commands:\n");
	printf(" 1 - 'concurrent connections'\n");
	printf(" 2 - 'historical accesses'\n");
	printf(" 3 - 'transfered bytes'\n");
	printf(" 4 - 'filtered mails'\n");
	printf(" 5 - 'get mediatypes'\n");
	printf(" 6 - 'get replacement message'\n");
	printf(" 7 - 'get filter command'\n");
	printf(" 8 - 'set mediatypes <mediatype1,mediatype2,...,mediatypesN>'\n");
	printf(" 9 - 'set replacement message <message>'\n");
	printf("10 - 'set filter command  <command>'\n");
	printf("11 - 'user <username>'\n");
	printf("12 - 'password <password>'\n");
	printf("13 - 'help'\n");
	printf("14 - 'exit'\n");
}
