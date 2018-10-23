#include <ctype.h>
#include <stdio.h>
#include <string.h>

#include "mediatypesfilter.h"
#include "stack.h"

int status;

void filteremail(mediatypetree* tree, char* filterMessage)
{
	int status = NEW_LINE_HEADER;
	char command[4096];
	//RFC 1341 says that a boundary can't be longer than 70 characters
	//char boundary[70] = {0};
	//Max line length is 80
	char header[80] = {0};
	//char mediatype[80] = {0};
	int indexHeader = 0;
	//stack* boundaryStack = initStack();

	while(status != END)
	{
		int n;
		n = scanf("%s", command);
		for(int i = 0; i < n; i++)
		{
			switch(status)
			{

				case NEW_LINE_HEADER:
					if(command[i] == '\n' || command[i] == '\r')
					{
						status = NEW_LINE_BODY;
						printf("\r\n");
					}
					else
					{
						status = CHECKING_HEADERS;
						printf("%c", command[i]);
					}
					break;

				case CHECKING_HEADERS:
					if(!isalpha(command[i]) || command[i] != '-')
					{
						if(command[i] == ':')
						{
							if(isContentTypeHeader(header))
							{
								status = CHECK_MEDIA_TYPE;
							}
							indexHeader = 0;
						}
					}
					else
					{
						indexHeader++;
						header[i] = command[i];
					}		
					break;

				case CHECK_MEDIA_TYPE:
					break;

				case BODY:
					if(command[i] == '\n' || command[i] == '\r')
						status = NEW_LINE_BODY;
					printf("%c\n", command[i]);
					break;

				case NEW_LINE_BODY:
					if(command[i] == '.')
						status = LINE_ONLY_DOT;
					else if(command[i] != '\n' && command[i] != '\r')
						status = BODY;
					break;

				case LINE_ONLY_DOT:
					if(command[i] == '\n' || command[i] == '\r')
						status = END;
					else
						status = BODY;
					break; 
			}
		}
	}

}

int isContentTypeHeader(char* header)
{
	int i=0;
	while(header[i] != 0){
		header[i] = tolower(header[i]);
		i++;
	}
	unsigned int length = strlen("content-type");
	return (strncmp(header,"content-type", length) == 0 && strlen(header) == length) ? 1 : 0;
}
