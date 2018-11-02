#include <ctype.h>
#include <string.h>
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
			switch(peekInt(context->actions))
			{
				/*Empieza una nueva linea, se verifica si hay line folding y si hace falta revisar headers
				relevantes.*/
				case NEW_LINE:

					popInt(context->actions);

					if(!isspace(buffer[i]))
					{
						/*No quedan headers relevantes*/
						if(!context->contenttypedeclared ||
						 (!context->encondingdeclared && context->censored) ||
						 !context->contentlengthdeclared ||
						 !context->contententmd5declared)
						{
							if(peekInt(context->actions) == CHECKING_CONTENT_TYPE)
							{
								context->contenttypedeclared = 1;
								checkmediatypes(context->ctp, ' ');
								if(context->ctp->matchfound)
								{
									write(STDOUT_FILENO, "text/plain\r\n", strlen("text/plain\r\n"));
								}
								else
								{
									write(STDOUT_FILENO, 
										context->ctp->startingIndex[context->ctp->lastmatch] + context->ctp->mediatypes,
										context->ctp->index);
									write(STDOUT_FILENO, "\r\n", 2);
									
								}
								context->ctp = NULL;
								popInt(context->actions);
							}
							i--;
							pushInt(context->actions, CHECKING_HEADER);
						}
						else
						{
							write(STDOUT_FILENO, buffer + i, 1);
						}
					}
					else if(isspace(buffer[i]) && buffer[i] != '\r')
					{
						if(peekInt(context->actions) == CHECKING_CONTENT_TYPE)
						{

						}
						write(STDOUT_FILENO, buffer + i, 1);
					}
					else if(buffer[i] == '\r')
					{
						if(peekInt(context->actions) == CHECKING_CONTENT_TYPE)
						{
							context->contenttypedeclared = 1;
							checkmediatypes(context->ctp, ' ');
							if(context->ctp->matchfound)
							{
								write(STDOUT_FILENO, "text/plain\r\n", strlen("text/plain\r\n"));
							}
							else
							{
								write(STDOUT_FILENO, 
									context->ctp->startingIndex[context->ctp->lastmatch] + context->ctp->mediatypes,
									context->ctp->index);
								write(STDOUT_FILENO, "\r\n", 2);
								
							}
							context->ctp = NULL;
							popInt(context->actions);
						}
						pushInt(context->actions, CARRY_RETURN_END_OF_HEADERS);
					}
					break;

				case CARRY_RETURN:
					popInt(context->actions);
					pushInt(context->actions, NEW_LINE);
					write(STDOUT_FILENO, buffer + i, 1);
					break;

				/*Se verifica si el content-type es censurable*/
				case CHECKING_CONTENT_TYPE:
					if(context->ctp == NULL)
					{
						context->ctp = malloc(sizeof(contentypevalidator));
						context->ctp = initcontenttypevalidator(censoredMediaTypes);
					}
					if(buffer[i] == '\r')
					{
						pushInt(context->actions, IGNORE_CARRY_RETURN);
					}
					if(context->ctp->matchfound == 1)
					{
						popInt(context->actions);
						pushInt(context->actions, WAIT_FOR_NEW_LINE);
						write(STDOUT_FILENO, "text/plain", strlen("text/plain"));
						context->censored = 1;
						context->contenttypedeclared = 1;
					}

					checkmediatypes(context->ctp, buffer[i]);

					if(!context->ctp->stillValid)
					{
						context->contenttypedeclared = 1;
						popInt(context->actions);
						pushInt(context->actions, CHECKING_BOUNDARY);
						write(STDOUT_FILENO, 
							context->ctp->startingIndex[context->ctp->lastmatch] + context->ctp->mediatypes,
							context->ctp->index);
						write(STDOUT_FILENO, buffer + i, 1);
					}
					break;

				/*Se verifica si hay un header relevante, es decir, Content-Transfer-Enconding o 
				Content-Type, en caso de ser Content-Type se manda a revisar el contenido, y en caso de 
				ser el primero se verifica si es censurable. En el caso que lo sea se propone un encoding 
				de 8-bit.*/
				case CHECKING_HEADER:
					if(context->hv == NULL)
					{
						context->hv = initheadervalidator();
					}
					checkheader(context->hv, buffer[i]);
					if(context->hv->matchfound != 0)
					{
						popInt(context->actions);
						if(context->hv->matchfound == CONTENT_TYPE)
						{
							pushInt(context->actions, CHECKING_CONTENT_TYPE);
							write(STDOUT_FILENO, "Content-type:", strlen("Content-type:"));
						}
						else if(context->hv->matchfound == CONTENT_TRANSFER_ENCONDING)
						{
							if(context->contenttypedeclared)
							{
								if(context->censored)
								{
									write(STDOUT_FILENO, "Content-Type-Enconding: 8-bit\r\n", 
										strlen("Content-Type-Enconding: 8-bit\r\n"));
									popInt(context->actions);
									pushInt(context->actions, IGNORE_UNTIL_NEW_LINE);
									context->encondingdeclared = 1;
								}
								else
								{
									/*Qué hacer cuando no se sabe si esta censurado.*/
								}
							}
							/*Ignora Content-Length y Content-MD5SUM, estos headers no son recomendados*/
							else
							{
								if(context->hv->matchfound == CONTENT_LENGTH)
								{
									context->contentlengthdeclared = 1;
								}
								else
								{
									context->contententmd5declared = 1;
								}
								pushInt(context->actions, IGNORE_UNTIL_NEW_LINE);
							}
						}
						else
						{
							pushInt(context->actions, IGNORE_UNTIL_NEW_LINE);
						}
						destroyheadervalidator(context->hv);
						context->hv = NULL;
					}
					else if(!context->hv->stillvalid)
					{
						write(STDOUT_FILENO, context->hv->headers[context->hv->lastmatch], 
							context->hv->index);
						write(STDOUT_FILENO, buffer + i, 1);
						context->hv = NULL;
						popInt(context->actions);
						pushInt(context->actions, WAIT_FOR_NEW_LINE);
					}
					break;

				/*Verifica el boundary y lo pushea a la pila de boundaries*/
				case CHECKING_BOUNDARY:
					if(context->bv == NULL)
					{
						context->bv = initboundaryvalidator();
					}
					if(context->bv->end)
					{
						pushString(context->boundaries, context->bv->boundary);
						context->bv = NULL;
						popInt(context->actions);
						pushInt(context->actions, WAIT_FOR_NEW_LINE);
					} 
					else if(!context->bv->stillvalid)
					{
						write(STDOUT_FILENO, "boundary", context->bv->index);
						write(STDOUT_FILENO, buffer + i, 1);
						context->bv = NULL;

					}
					break;

				case WAIT_FOR_NEW_LINE:
					if(buffer[i] == '\r')
					{
						popInt(context->actions);
						pushInt(context->actions, CARRY_RETURN); 
					}
					write(STDOUT_FILENO, buffer + i, 1);
					break;

				case IGNORE_UNTIL_NEW_LINE:
					if(buffer[i] == '\r')
					{
						popInt(context->actions);
						pushInt(context->actions, IGNORE_CARRY_RETURN);
					}
					break;


				case IGNORE_CARRY_RETURN:
					popInt(context->actions);
					pushInt(context->actions, NEW_LINE);
					break;

				case CARRY_RETURN_END_OF_HEADERS:
					write(STDOUT_FILENO, "\n", 1);
					popInt(context->actions);
					if(context->censored)
					{
						pushInt(context->actions, IGNORE_UNTIL_BOUNDARY);
					}
					else
					{
						if(context->multipart)
						{
							write(STDOUT_FILENO, "--", 2);
							char* aux = peekString(context->boundaries);
							write(STDOUT_FILENO, aux, strlen(aux));
							pushInt(context->actions, IGNORE_BOUNDARY);
						}
						else
						{
							restartcontext(context);
							pushInt(context->actions, WAIT_UNTIL_BOUNDARY);
						}	
					}
					break;

				case IGNORE_BOUNDARY:
					if(buffer[i] == '\n')
					{
						popInt(context->actions);
						pushInt(context->actions, CHECKING_HEADER);

						write(STDOUT_FILENO, "\r\n", 2);
					}
					break;

				case WAIT_UNTIL_BOUNDARY:

					write(STDOUT_FILENO, buffer + i, 1);

					if(context->bc == NULL)
					{
						char* aux = peekString(context->boundaries);
						context->bc = malloc(sizeof(boundarycomparator));
						context->bc = initboundarycomparator(aux);
						restartcontext(context);
					}

					if(buffer[i] == '\r')
					{
						if(context->bc->match)
						{
							restartcontext(context);
							context->bc = NULL;
							if(context->bc->endingboundary)
							{
								write(STDOUT_FILENO, "--", 2);
								popString(context->boundaries);
							}
							write(STDOUT_FILENO, "\r\n", 2);
							popInt(context->actions);
							pushInt(context->actions, CARRY_RETURN_BODY);
						}
					}

					compareboundaries(context->bc, buffer[i]);

					if(!context->bc->stillvalid)
					{
						pushInt(context->actions, WAIT_UNTIL_NEW_LINE_BODY);
						context->bc = NULL;
					}

					break;

				case WAIT_UNTIL_NEW_LINE_BODY:
					if(buffer[i] == '\n')
					{
						popInt(context->actions);
					}
					write(STDOUT_FILENO, buffer + i, 1);

				case IGNORE_UNTIL_BOUNDARY:
					
					if(context->bc == NULL)
					{
						char* aux = peekString(context->boundaries);
						context->bc = malloc(sizeof(boundarycomparator));
						context->bc = initboundarycomparator(aux);
						restartcontext(context);
					}

					if(buffer[i] == '\r')
					{
						if(context->bc->match)
						{
							restartcontext(context);
							context->bc = NULL;
							write(STDOUT_FILENO, "--", 2);
							write(STDOUT_FILENO, context->bc->boundary, context->bc->boundarylength);
							if(context->bc->endingboundary)
							{
								write(STDOUT_FILENO, "--", 2);
								popString(context->boundaries);
							}
							write(STDOUT_FILENO, "\r\n", 2);
							popInt(context->actions);
							pushInt(context->actions, CARRY_RETURN_BODY);
						}
					}

					compareboundaries(context->bc, buffer[i]);

					if(!context->bc->stillvalid)
					{
						pushInt(context->actions, IGNORE_UNTIL_NEW_LINE_BODY);
						context->bc = NULL;
					}

					break;

				case IGNORE_UNTIL_NEW_LINE_BODY:
					if(buffer[i] == '\r')
					{
						popInt(context->actions);
						pushInt(context->actions, IGNORE_CARRY_RETURN_BODY);
					}
					break;

				case IGNORE_CARRY_RETURN_BODY:
					popInt(context->actions);
					break;

				/*Imprimo todo lo que hay en el buffer*/
				case WAIT_UNTIL_END:
					write(STDOUT_FILENO, buffer + i, n - i);
					i = n;
					break;

				case IGNORE_UNTIL_END:
					return;
			}
		}
	}
	while(n > 0);
}

ctx* initcontext(char* censoredMediaTypes)
{
	ctx* context = malloc(sizeof(context));
	context->actions = malloc(sizeof(stackint));
	context->actions = initIntStack();
	pushInt(context->actions, NEW_LINE);
	context->boundaries = malloc(sizeof(stackstring));
	context->boundaries = initStringStack();
	context->contenttypedeclared = 0;
	context->censored = 0;
	context->ctp = NULL;
	context->hv = NULL;
	context->bv = NULL;
	context->bc = NULL;
	return context;
}

void destroycontext(ctx* context)
{
	free(context->ctp);
	free(context->hv);
	free(context->bv);
	free(context);
}

void restartcontext(ctx* context)
{
	context->censored = 0;
	context->bc = NULL;
	context->contenttypedeclared = 0;
	context->encondingdeclared = 0;
	context->contentlengthdeclared = 0;
	context->contententmd5declared = 0;
	context->multipart = 0;
	context->message = 0;
}
