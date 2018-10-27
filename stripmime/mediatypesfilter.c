#include <ctype.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>

#include "mediatypesfilter.h"
#include "contenttypevalidator.h"


void filteremail(char* censoredMediaTypes, char* filterMessage)
{
	ctx* context = initcontext();

	int n = 0;
	char buffer[4096];
	do
	{
		n = read(STDIN_FILENO, buffer, 4096);
		for(int i = 0; i < n; i++)
		{
			switch(context->action)
			{
				case NEW_LINE:
					if(buffer[i] != ' ')
					{
						context->action = CHECKING_HEADER;
					}
					else
					{
						context->action = context->lastAction;
					}
					break;
				case CHECKING_CONTENT_TYPE:
					
					if(context->ctp == NULL)
					{
						context->ctp = malloc(sizeof(contentypevalidator));
						context->ctp = initcontenttypevalidator(censoredMediaTypes);
					}
					if(context->ctp->matchfound == 1)
					{
					}
					else if(context->ctp->stillValid)
					{
						if(buffer[i] == ';')
						{

						}
						else
						{
							checkmediatypes(context->ctp, buffer[i]);
						}
					}
					break;
			}
		}
	}
	while(n > 0);
}

ctx* initcontext(char* censoredMediaTypes)
{
	ctx* context = malloc(sizeof(context));
	context->action = NEW_LINE;
	context->lastAction= -1;
	context->ctp = NULL;
	return context;
}

