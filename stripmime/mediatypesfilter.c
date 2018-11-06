#include <ctype.h>
#include <string.h>
#include <stdlib.h>
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
						nolinefoldinganalisis(context, buffer, i);
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
					/*Hay line folding, no hay nuevo header*/
					else if(isspace(buffer[i]) && buffer[i] != '\r')
					{
						/*Me fijo si el estado actual corresponde a un header que no quiero 
						o no se si debo mostrar el valor.*/
						if(peekInt(context->actions) != CHECKING_CONTENT_TYPE &&
							peekInt(context->actions) != CHECKING_TRANSFER_ENCODING &&
							peekInt(context->actions) != IGNORE_UNTIL_NEW_LINE){
							write(STDOUT_FILENO, buffer + i, 1);
							if(peekInt(context->actions) == -1)
							{
								pushInt(context->actions, WAIT_FOR_NEW_LINE);
							}
						}
						/*Si es el valor del header content-transfer-encoding lo voy a querer guardar
						para el futuro*/
						else if(peekInt(context->actions) == CHECKING_TRANSFER_ENCODING)
						{
							addchar(context->extra, buffer[i]);
						}
					}
					/*Llego al body del mail*/
					else if(buffer[i] == '\r')
					{
						nolinefoldinganalisis(context, buffer, i);
						pushInt(context->actions, CARRY_RETURN_END_OF_HEADERS);
					}
					break;

				/*Se verifica si el content-type es censurable*/
				case CHECKING_CONTENT_TYPE:
					/*Inicializa el parser*/
					if(context->ctp == NULL)
					{
						context->ctp = initcontenttypevalidator(censoredMediaTypes);
						context->extra = initextrainformation(5);
					}

					if(buffer[i] == '\r')
					{
						pushInt(context->actions, IGNORE_CARRY_RETURN);
						break;
					}	

					checkmediatypes(context->ctp, buffer[i]);
					addchar(context->extra, buffer[i]);

					/*El content-type es censurable*/
					if(context->ctp->matchfound == NORMAL_MATCH)
					{
						write(STDOUT_FILENO, "text/plain;\r\n", strlen("text/plain;\r\n"));
						context->contenttypedeclared = 1;
						censored = 1;
						popInt(context->actions);
						pushInt(context->actions, IGNORE_UNTIL_NEW_LINE);
						context->extra = NULL;
						break;
					}
					/*La maquina de estados señala que no es censurable*/
					else if(!context->ctp->stillValidCensored)
					{
						if(context->ctp->matchfound == MULTIPART_MATCH)
						{
							multipart = 1;
							context->contenttypedeclared = 1;
							popInt(context->actions);
							pushInt(context->actions, CHECKING_BOUNDARY);
							write(STDOUT_FILENO, context->extra->buff, context->extra->size);
							context->extra = NULL;
						}
						else if(context->ctp->matchfound == MESSAGE_MATCH)
						{
							message = 1;
							context->contenttypedeclared = 1;
							popInt(context->actions);
							pushInt(context->actions, WAIT_FOR_NEW_LINE);
							write(STDOUT_FILENO, context->extra->buff, context->extra->size);
							context->extra = NULL;
						}
						/*No se multipart, ni message, ni cenurable, entonces, se verifica
						si ya fue declarado su tranfer-encoding*/
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
							write(STDOUT_FILENO, context->extra->buff, context->extra->size);
							context->extra = NULL;
						}
					}				
					break;

				/*Imprime hasta al final el content-type, luego imprime el encoding*/
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
						context->extra = NULL;
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
						context->extra = initextrainformation(5);
					}

					checkheader(context->hv, buffer[i]);
					addchar(context->extra, buffer[i]);

					/**Busca coincidencia con header relevante*/
					if(context->hv->matchfound != 0)
					{
						popInt(context->actions);
						if(context->hv->matchfound == CONTENT_TYPE)
						{
							pushInt(context->actions, CHECKING_CONTENT_TYPE);
							write(STDOUT_FILENO, context->extra->buff, context->extra->size);
						}
						else if(context->hv->matchfound == CONTENT_TRANSFER_ENCONDING)
						{
							if(context->contenttypedeclared)
							{
								/*Si esta censurado el valor por default es 8-bit*/
								if(censored)
								{
									write(STDOUT_FILENO, "Content-Transfer-Enconding: 8-bit\r\n", 
										strlen("Content-Transfer-Enconding: 8-bit\r\n"));
									popInt(context->actions);
									pushInt(context->actions, IGNORE_UNTIL_NEW_LINE);
									context->encondingdeclared = 1;
								}
								else
								{
									write(STDOUT_FILENO, context->extra->buff, context->extra->size);
									popInt(context->actions);
									pushInt(context->actions, WAIT_FOR_NEW_LINE);
									context->encondingdeclared = 1;
								}
							}
							else
							{
								popInt(context->actions);
								context->encondingdeclared = 1;
								pushInt(context->actions, CHECKING_TRANSFER_ENCODING);
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
						context->extra = NULL;
					}
					else if(!context->hv->stillvalid)
					{
						write(STDOUT_FILENO, context->extra->buff, context->extra->size);
						context->hv = NULL;
						context->extra = NULL;
						popInt(context->actions);
						pushInt(context->actions, WAIT_FOR_NEW_LINE);
					}
					break;

				/*Verifica el boundary del multipart y lo pushea a la pila de boundaries*/
				case CHECKING_BOUNDARY:
					if(context->bv == NULL)
					{
						context->bv = initboundaryvalidator();
						context->extra = initextrainformation(5);
					}

					checkboundary(context->bv, buffer[i]);
					addchar(context->extra, buffer[i]);

					/*Si ya finalizo se pushea en la pila el boundary encontrado*/
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
					/*En caso de no ser boundary, o no dar referencias de serlo. Se imprime hasta punto y
					coma*/ 
					else if(!context->bv->stillvalid)
					{
						endextrainformation(context->extra);
						write(STDOUT_FILENO, context->extra->buff, context->extra->size);
						context->extra = NULL;
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
						addchar(context->extra, '\n');
						endextrainformation(context->extra);
						write(STDOUT_FILENO, context->extra->buff, context->extra->size);
						context->extra = NULL;
						popInt(context->actions);
						pushInt(context->actions, IGNORE_CARRY_RETURN); 
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
						addchar(context->extra, '\n');
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

				/*Pasaje al body y elecccion de que hacer*/
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

				/* Imprime hasta que aparece el boundary esperado */
				case WAIT_UNTIL_BOUNDARY:

					if(context->bc == NULL)
					{
						restartcontext(context);
						char* aux = peekString(context->boundaries);
						context->bc = malloc(sizeof(boundarycomparator));
						context->bc = initboundarycomparator(aux);
						context->extra = initextrainformation(5);
					}

					/*Se fija en la maquina de estados que siga siendo posible que la linea actual
					sea el boundary*/
					compareboundaries(context->bc, buffer[i]);
					addchar(context->extra, buffer[i]);
					if(buffer[i] == '\r')
					{
						boundaryatcarryreturn(context);
					}
					else if(!context->bc->stillvalid)
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
						context->bc = initboundarycomparator(aux);
					}

					compareboundaries(context->bc, buffer[i]);

					if(buffer[i] == '\r')
					{
						boundaryatcarryreturn(context);
					}
					else if(!context->bc->stillvalid)
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

/*Se opera sobre algunos estados que solo finalizan al empezar un nuevo header,
y en el caso que haya line folding estos estados deben permanecer activos*/
void nolinefoldinganalisis(ctx* context, char* buffer, int i)
{
	if(peekInt(context->actions) == IGNORE_UNTIL_NEW_LINE)
	{
		popInt(context->actions);
	}
	/*Termino de imprimir el content-type e imprimo el encoding*/
	else if(peekInt(context->actions) == PRINT_TRANSFER_ENCODING)
	{
		popInt(context->actions);
		write(STDOUT_FILENO, "Content-Transfer-Enconding: ", strlen("Content-Transfer-Enconding: "));
		write(STDOUT_FILENO, context->encondingselected, strlen(context->encondingselected));
	}
	/*Analizo si el content-type debe ser censurable o no*/
	else if(peekInt(context->actions) == CHECKING_CONTENT_TYPE)
	{
		popInt(context->actions);
		checkmediatypes(context->ctp, '\r');
		/*Como lo es verifico que sea un match para censurar*/
		if(context->ctp->matchfound == NORMAL_MATCH)
		{
			write(STDOUT_FILENO, "text/plain;\r\n", strlen("text/plain;\r\n"));
			context->contenttypedeclared = 1;
			censored = 1;
			
		}
		else
		{
			/*Imprimo todo lo que consumi previamente*/
			write(STDOUT_FILENO, context->extra->buff, context->extra->size);
			write(STDOUT_FILENO, "\r\n", 2);
			context->contenttypedeclared = 1;
			if(context->encondingdeclared)
			{
				write(STDOUT_FILENO, context->encondingselected, 
					strlen(context->encondingselected));
				write(STDOUT_FILENO, "\r\n", 2);
			}
		}
		context->extra = NULL;
	}
	else if(peekInt(context->actions) == CHECKING_TRANSFER_ENCODING)
	{
		endextrainformation(context->extra);
		context->encondingselected = malloc(context->extra->size + 1);
		strcpy(context->encondingselected, context->extra->buff);
		context->extra = NULL;
		popInt(context->actions);
	}	
}

/* Las acciones a validar para saber si el boundary es el de la linea
y las acciones a realizar luego, sea o no . */
int boundaryatcarryreturn(ctx* context)
{
	int ret = context->bc->match;
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
	}
	else
	{
		pushInt(context->actions, IGNORE_CARRY_RETURN_BODY);
		restartcontext(context);
	}

	return ret;
}