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
	char buffer[4096] = {0};
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
						i--;
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
						checkmediatypes(context->ctp, buffer[i]);
					}
					break;

				case CHECKING_HEADER:
					if(context->hv == NULL)
					{
						context->hv = initheadervalidator();
					}
					checkheader(context->hv, buffer[i]);
					if(context->hv->matchfound != 0)
					{
						if(context->hv->matchfound == CONTENT_TYPE)
						{
							context->action = CHECKING_CONTENT_TYPE;
						}
						else if(context->hv->matchfound == MIME_VERSION)
						{
							;
						}
						destroyheadervalidator(context->hv);
						context->hv = NULL;
					}
					break;

				case CHECKING_BOUNDARY:
					if(context->bv == NULL)
					{
						context->bv = initboundaryvalidator();
					} 
					break;
			}
		}
	}
	while(n > 0);
	destroycontext(context);
}

ctx* initcontext(char* censoredMediaTypes)
{
	ctx* context = malloc(sizeof(context));
	context->action = NEW_LINE;
	context->lastAction= -1;
	context->ctp = NULL;
	context->hv = NULL;
	context->bv = NULL;
	return context;
}

void destroycontext(ctx* context)
{
	free(context->ctp);
	free(context->hv);
	free(context);
}
