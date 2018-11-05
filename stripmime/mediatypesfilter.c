#include <ctype.h>
#include <string.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>

#include "mediatypesfilter.h"
#include "contenttypevalidator.h"
#include "boundaryvalidator.h"

int censored = 0;
int multipart = 0;
int message = 0;

void filteremail(char* censoredMediaTypes, char* fm)
{
	char* filterMessage = bytestuffmessage(fm);
	ctx* context = initcontext();
	pushInt(context->actions, NEW_LINE);
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
					/*No hay line folding, es un nuevo header*/
					if(!isspace(buffer[i]))
					{
						/*Es nueva linea, entonces, se debe analizar de nuevo.*/
						if(peekInt(context->actions) == IGNORE_UNTIL_NEW_LINE)
						{
							popInt(context->actions);
						}
						/*Analizo el content-type*/
						else if(peekInt(context->actions) == CHECKING_CONTENT_TYPE)
						{
							popInt(context->actions);
							checkmediatypes(context->ctp, '\r');
							if(context->ctp->matchfound == NORMAL_MATCH)
							{
								write(STDOUT_FILENO, "text/plain;\r\n", strlen("text/plain;\r\n"));
								context->contenttypedeclared = 1;
								censored = 1;
								
							}
							else
							{
								write(STDOUT_FILENO, 
									context->ctp->startingIndex[context->ctp->lastmatch] + context->ctp->mediatypes,
									context->ctp->index);
								write(STDOUT_FILENO, "\r\n", 2);
								context->contenttypedeclared = 1;
								if(context->encondingdeclared)
								{
									write(STDOUT_FILENO, context->encondingselected, 
										strlen(context->encondingselected));
									write(STDOUT_FILENO, "\r\n", 2);
								}
							}
						}
						/*No quedan headers relevantes*/
						if(!context->contenttypedeclared ||
						 (!context->encondingdeclared && censored) ||
						 !context->contentlengthdeclared ||
						 !context->contententmd5declared)
						{
							if(peekInt(context->actions) == CHECKING_TRANSFER_ENCODING)
							{
								endextrainformation(context->extra);
								context->encondingselected = context->extra->buff;
								context->extra = NULL;
							}
							i--;
							pushInt(context->actions, CHECKING_HEADER);
						}
						else
						{
							pushInt(context->actions, WAIT_FOR_NEW_LINE);
							write(STDOUT_FILENO, buffer + i, 1);
						}
					}
					/*Hay line folding, no hay nueva linea*/
					else if(isspace(buffer[i]) && buffer[i] != '\r')
					{
						/*Solo se popea ya que corresponde a line folding, por lo tanto,
						se vuelve a la accion anterior.*/
						if(peekInt(context->actions) != CHECKING_CONTENT_TYPE &&
							peekInt(context->actions) != CHECKING_TRANSFER_ENCODING){
							write(STDOUT_FILENO, buffer + i, 1);
							if(peekInt(context->actions) == -1)
							{
								pushInt(context->actions, WAIT_FOR_NEW_LINE);
							}
						}
					}
					else if(buffer[i] == '\r')
					{
						if(peekInt(context->actions) == CHECKING_TRANSFER_ENCODING)
						{
							endextrainformation(context->extra);
							context->encondingselected = context->extra->buff;
							context->extra = NULL;
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
						break;
					}	

					checkmediatypes(context->ctp, buffer[i]);

					if(context->ctp->matchfound == NORMAL_MATCH)
					{
						write(STDOUT_FILENO, "text/plain;\r\n", strlen("text/plain;\r\n"));
						context->contenttypedeclared = 1;
						censored = 1;
						popInt(context->actions);
						pushInt(context->actions, IGNORE_UNTIL_NEW_LINE);
						break;
					}
					else if(!context->ctp->stillValidCensored)
					{
						if(context->ctp->matchfound == MULTIPART_MATCH)
						{
							multipart = 1;
							context->contenttypedeclared = 1;
							popInt(context->actions);
							pushInt(context->actions, CHECKING_BOUNDARY);
							write(STDOUT_FILENO, "multipart/", strlen("multipart/"));
							break;
						}
						else if(context->ctp->matchfound == MESSAGE_MATCH)
						{
							message = 1;
							context->contenttypedeclared = 1;
							popInt(context->actions);
							pushInt(context->actions, WAIT_FOR_NEW_LINE);
							write(STDOUT_FILENO, "message/", strlen("message/"));
							break;
						}
						else if(!context->ctp->stillValidExtras)
						{
							context->contenttypedeclared = 1;
							popInt(context->actions);
							if(!context->encondingdeclared)
							{
								pushInt(context->actions, WAIT_FOR_NEW_LINE);
							}
							else
							{
								pushInt(context->actions, PRINT_TRANSFER_ENCODING);
							}
							write(STDOUT_FILENO, 
								context->ctp->startingIndex[context->ctp->lastmatch] + context->ctp->mediatypes,
								context->ctp->index);
							write(STDOUT_FILENO, buffer + i, 1);
						}
					}				
					break;

				case PRINT_TRANSFER_ENCODING:
					if(context->extra == NULL)
					{
						context->extra = initextrainformation(5);
					}
					addchar(context->extra, buffer[i]);
					if(buffer[i] == '\r')
					{
						addchar(context->extra, '\n');
						endextrainformation(context->extra);
						write(STDOUT_FILENO, context->extra->buff, context->extra->size);
						write(STDOUT_FILENO, "Content-Transfer-Enconding: ", strlen("Content-Transfer-Enconding: "));
						write(STDOUT_FILENO, context->encondingselected, strlen(context->encondingselected));
						write(STDOUT_FILENO, "\r\n", 2);
						context->extra = NULL;
						popInt(context->actions);
						pushInt(context->actions, IGNORE_CARRY_RETURN); 
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
								if(censored)
								{
									write(STDOUT_FILENO, "Content-Type-Enconding: 8-bit\r\n", 
										strlen("Content-Type-Enconding: 8-bit\r\n"));
									popInt(context->actions);
									pushInt(context->actions, IGNORE_UNTIL_NEW_LINE);
									context->encondingdeclared = 1;
								}
								else
								{
									popInt(context->actions);
									pushInt(context->actions, CHECKING_TRANSFER_ENCODING);
									context->encondingdeclared = 1;
								}
							}
						}
						else
						{
							/*Ignora Content-Length y Content-MD5SUM, estos headers no son recomendados*/
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
						destroyheadervalidator(context->hv);
						context->hv = NULL;
					}
					else if(!context->hv->stillvalid)
					{
						write(STDOUT_FILENO, context->hv->headers[context->hv->lastmatch], 
							context->hv->index);
						write(STDOUT_FILENO, buffer + i, 1);
						context->hv = NULL;
						context->extra = NULL;
						popInt(context->actions);
						pushInt(context->actions, WAIT_FOR_NEW_LINE);
					}
					break;

				/*Verifica el boundary y lo pushea a la pila de boundaries*/
				case CHECKING_BOUNDARY:
					if(context->bv == NULL)
					{
						context->bv = initboundaryvalidator();
						context->extra = initextrainformation(5);
					}

					checkboundary(context->bv, buffer[i]);
					addchar(context->extra, buffer[i]);

					if(context->bv->end)
					{
						endextrainformation(context->extra);
						write(STDOUT_FILENO, context->extra->buff, context->extra->size);
						context->extra = NULL;
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
						pushInt(context->actions, WAIT_FOR_DOT_COMMA);

					}
					break;

				case WAIT_FOR_NEW_LINE:
					if(context->extra == NULL)
					{
						context->extra = initextrainformation(5);
					}
					addchar(context->extra, buffer[i]);
					if(buffer[i] == '\r')
					{
						endextrainformation(context->extra);
						write(STDOUT_FILENO, context->extra->buff, context->extra->size);
						context->extra = NULL;
						popInt(context->actions);
						pushInt(context->actions, CARRY_RETURN); 
					}
					break;

				case CHECKING_TRANSFER_ENCODING:
					if(context->extra == NULL)
					{
						context->extra = initextrainformation(5);
					}
					addchar(context->extra, buffer[i]);
					if(buffer[i] == '\r')
					{
						pushInt(context->actions, IGNORE_CARRY_RETURN);
						addchar(context->extra, '\n');
					}
					break;

				case WAIT_FOR_DOT_COMMA:
					if(context->extra == NULL)
					{
						context->extra = initextrainformation(5);
					}
					addchar(context->extra, buffer[i]);
					if(buffer[i] == '\r')
					{
						pushInt(context->actions, CARRY_RETURN);
						endextrainformation(context->extra);
						write(STDOUT_FILENO, context->extra->buff, context->extra->size);
						context->extra = NULL;

					}
					else if(buffer[i] == ';')
					{
						popInt(context->actions);
						endextrainformation(context->extra);
						write(STDOUT_FILENO, context->extra->buff, context->extra->size);
						context->extra = NULL;						
					}

					break;

				case IGNORE_UNTIL_NEW_LINE:
					if(buffer[i] == '\r')
					{
						pushInt(context->actions, IGNORE_UNTIL_NEW_LINE);
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
					if(censored)
					{
						write(STDOUT_FILENO, filterMessage, strlen(filterMessage));
						write(STDOUT_FILENO, "\r\n", 2);
						if(peekString(context->boundaries) != NULL)
						{
							pushInt(context->actions, IGNORE_UNTIL_BOUNDARY);
						}
						else
						{
							pushInt(context->actions, IGNORE_UNTIL_END);
						}
					}
					else
					{
						if(multipart)
						{
							write(STDOUT_FILENO, "--", 2);
							char* aux = peekString(context->boundaries);
							write(STDOUT_FILENO, aux, strlen(aux));
							pushInt(context->actions, IGNORE_BOUNDARY);
						}
						else if(message)
						{
							pushInt(context->actions, CHECKING_HEADER);
						}
						else
						{
							pushInt(context->actions, WAIT_UNTIL_BOUNDARY);
						}
						restartcontext(context);	
					}
					break;

				case IGNORE_BOUNDARY:
					if(buffer[i] == '\n')
					{
						popInt(context->actions);
						pushInt(context->actions, CHECKING_HEADER);
						restartcontext(context);
						write(STDOUT_FILENO, "\r\n", 2);
					}
					break;

				case WAIT_UNTIL_BOUNDARY:

					if(context->bc == NULL)
					{
						restartcontext(context);
						char* aux = peekString(context->boundaries);
						context->bc = malloc(sizeof(boundarycomparator));
						context->bc = initboundarycomparator(aux);
						context->extra = initextrainformation(5);
					}

					compareboundaries(context->bc, buffer[i]);
					addchar(context->extra, buffer[i]);
					if(buffer[i] == '\r')
					{
						if(context->bc->match)
						{
							write(STDOUT_FILENO, "--", 2);
							write(STDOUT_FILENO, context->bc->boundary, context->bc->boundarylength);
							popInt(context->actions);
							if(context->bc->endingboundary)
							{
								write(STDOUT_FILENO, "--\r\n", 4);
								popString(context->boundaries);
								if(peekString(context->boundaries) == NULL)
								{
									pushInt(context->actions, WAIT_UNTIL_END);
								}
								else
								{
									pushInt(context->actions, WAIT_UNTIL_BOUNDARY);
								}
							}
							else
							{
								write(STDOUT_FILENO, "\r\n", 2);
								pushInt(context->actions, CHECKING_HEADER);
							}
							pushInt(context->actions, IGNORE_CARRY_RETURN_BODY);
							restartcontext(context);
							break;
						}
					}

					if(!context->bc->stillvalid)
					{
						endextrainformation(context->extra);
						write(STDOUT_FILENO, context->extra->buff, context->extra->size);
						pushInt(context->actions, WAIT_UNTIL_NEW_LINE_BODY);
						restartcontext(context);
					}

					break;

				case WAIT_UNTIL_NEW_LINE_BODY:
					if(context->extra == NULL)
					{
						context->extra = initextrainformation(5);
					}

					addchar(context->extra, buffer[i]);

					if(buffer[i] == '\n')
					{
						endextrainformation(context->extra);
						write(STDOUT_FILENO, context->extra->buff, context->extra->size);
						popInt(context->actions);
					}
					break;
					

				case IGNORE_UNTIL_BOUNDARY:
					if(context->bc == NULL)
					{
						restartcontext(context);
						char* aux = peekString(context->boundaries);
						context->bc = malloc(sizeof(boundarycomparator));
						context->bc = initboundarycomparator(aux);
					}

					compareboundaries(context->bc, buffer[i]);
					if(buffer[i] == '\r')
					{
						if(context->bc->match)
						{
							write(STDOUT_FILENO, "--", 2);
							write(STDOUT_FILENO, context->bc->boundary, context->bc->boundarylength);
							popInt(context->actions);
							if(context->bc->endingboundary)
							{
								write(STDOUT_FILENO, "--\r\n", 4);
								popString(context->boundaries);
								if(peekString(context->boundaries) == NULL)
								{
									pushInt(context->actions, WAIT_UNTIL_END);
								}
								else
								{
									pushInt(context->actions, WAIT_UNTIL_BOUNDARY);
								}
							}
							else
							{
								write(STDOUT_FILENO, "\r\n", 2);
								pushInt(context->actions, CHECKING_HEADER);
							}
							pushInt(context->actions, IGNORE_CARRY_RETURN_BODY);
							restartcontext(context);
							break;
						}
						else
						{
							pushInt(context->actions, IGNORE_CARRY_RETURN_BODY);
							restartcontext(context);
							break;
						}
					}

					if(!context->bc->stillvalid)
					{
						pushInt(context->actions, IGNORE_UNTIL_NEW_LINE_BODY);
						restartcontext(context);
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
	context->actions = initIntStack();
	context->boundaries = malloc(sizeof(stackstring));
	context->boundaries = initStringStack();
	context->contenttypedeclared = 0;
	context->ctp = NULL;
	context->hv = NULL;
	context->bv = NULL;
	context->bc = NULL;
	context->extra = NULL;
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
	multipart = 0;
	censored = 0;
	message = 0;
	context->bc = NULL;
	context->ctp = NULL;
	context->hv = NULL;
	context->extra = NULL;
	context->contenttypedeclared = 0;
	context->encondingdeclared = 0;
	context->contentlengthdeclared = 0;
	context->contententmd5declared = 0;
}

char* bytestuffmessage(char* fm)
{
	int i,j;
	i = j = 0;
	while(fm[i] != 0)
	{
		if(fm[i] == ' ')
		{
			j++;
		}
		i++;
	}
	/*Tamaño maximo que puede tener la palabra, en el caso que todas las nuevas lineas
	empiecen con .*/
	char* ret = malloc((i + j + 1) * sizeof(char));
	i = 0;
	j = 0;
	if(fm[0] == '.')
	{
		ret[0] = '.';
		ret[1] = '.';
		i=1;
		j=2;
	}
	while(fm[i] != 0)
	{
		ret[j] = fm[i];
		if(fm[i] == '\n')
		{
			i++;
			j++;
			if (fm[i] == '.')
			{
				ret[j] = '.';
				j++;
			}
			ret[j] = fm[i]; 
		}
		i++;
		j++;
	}

	return (char*)realloc(ret, (j+1)*sizeof(char));

}