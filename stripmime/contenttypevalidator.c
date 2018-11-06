#include <stdlib.h>
#include <stdio.h>
#include <ctype.h>

#include "contenttypevalidator.h"

contentypevalidator* initcontenttypevalidator(char* mts)
{
	contentypevalidator* ctp = malloc(sizeof(contentypevalidator));
	ctp->mediatypes = mts;
	
	int i = 0;
	if(mts[0] == 0)
	{
		ctp->quantityMediaTypes = 0;
		ctp->startingIndex = NULL;
		ctp->stillValidCensored = 0;
		ctp->stillValidExtras = 0;
		ctp->extramediatypes[0] = "multipart";
		ctp->extramediatypes[1] = "message";
		return ctp;
	}

	int counter = 1;

	while(mts[i] != 0)
	{
		if(mts[i] == ',')
		{
			counter++;
		}
		i++;
	}

	ctp->stillValidCensored = 1;
	ctp->stillValidExtras = 1;
	ctp->quantityMediaTypes = counter;
	ctp->isValidCensored = malloc(counter * sizeof(int));
	ctp->isValidExtras = malloc(2 * sizeof(int));
	ctp->startingIndex = malloc(counter * sizeof(int));
	ctp->extramediatypes = malloc(2 * sizeof(char*));
	ctp->extramediatypes[0] = "multipart";
	ctp->extramediatypes[1] = "message";
	ctp->ignore = 0;
	ctp->index = 0;
	ctp->matchfound = 0;
	ctp->lastmatch = 0;

	for(i = 0; i < counter; i++)
	{
		ctp->isValidCensored[i] = 1;
	}

	for(i = 0; i < 2; i++)
	{
		ctp->isValidExtras[i] = 1;
	}

	i=0;
	int j = 0;
	while(mts[i] != 0)
	{
		if(mts[i] == ',')
		{
			ctp->startingIndex[j] = i+1;
			j++;
		}
		i++;
	}

	return ctp;
}

void destroycontenttypevalidator(contentypevalidator* ctp)
{
	free(ctp->mediatypes);
	free(ctp->isValidCensored);
	free(ctp->isValidExtras);
	free(ctp->extramediatypes);
	free(ctp->startingIndex);
	free(ctp);
	return;
}

int checkmediatypes(contentypevalidator* ctp, char c)
{
	/*Señala fin de comentario si se cierra el parentesis*/
	if(ctp->ignore)
	{
		if(c == ')')
		{
			ctp->ignore = 0;
		}
		return 0;
	}
	/*Señala inicio de comentario*/
	if(c == '(')
	{
		ctp->ignore = 1;
		return 0;
	}
	else if(c == ' ')
	{
		return 0;
	}

	/*Se asume que ya dejan de ser valido los censurables. 
	Maquina de estados de los content-type censurables*/
	ctp->stillValidCensored = 0;
	for(int i = 0; i < ctp->quantityMediaTypes; i++)
	{
		/*Se pregunta si el siguiente content-type a verificar es o no
		valido, es decir, si puede matchear con el que esta siendo analizado*/
		if(ctp->isValidCensored[i])
		{
			ctp->lastmatch = i;
			/*Caracter a comparar en los media types*/
			int carachter = ctp->startingIndex[i] + ctp->index;
			/*Se fija si es wildcard*/
			if(ctp->mediatypes[carachter] == '*')
			{
				ctp->matchfound = NORMAL_MATCH;
				return 1;
			}
			/*Se fija si finaliza el content-type y hay match por ser exactamente el mismo*/
			else if((c == ';' || isspace(c)) && (ctp->mediatypes[carachter] == 0 ||
				ctp->mediatypes[carachter] == ','))
			{
				ctp->matchfound = NORMAL_MATCH;
				return 1;
			}
			if(ctp->mediatypes[carachter] == ',' || ctp->mediatypes[carachter] == 0 ||
				tolower(ctp->mediatypes[carachter]) != tolower(c))
			{
				ctp->isValidCensored[i] = 0;
			}
			else if(ctp->mediatypes[carachter] != c)
			{
				ctp->isValidCensored[i] = 0;
			}
			else
			{
				ctp->stillValidCensored = 1;
			}
		}
	}


	/*Misma logica que el de censurables, sin embargo, este no tiene wildcard y
	no se delimitan por comas, sino que es un array de char* */

	ctp->stillValidExtras = 0;

	for(int i = 0; i < 2; i++)
	{
		if(ctp->isValidExtras[i])
		{
			if(ctp->extramediatypes[i][ctp->index] == c)
			{
				ctp->stillValidExtras = 1;
			}
			else if(ctp->extramediatypes[i][ctp->index] == 0 &&
				c == '/')
			{
				ctp->matchfound = 2 + i;
			}
			else
			{
				ctp->isValidExtras[i] = 0;
			}
		}
	}

	/*En caso de seguir siendo valido se incrementa el indice*/

	if(ctp->stillValidExtras || ctp->stillValidCensored)
		(ctp->index)++;

	return (ctp->stillValidExtras || ctp->stillValidCensored);
}
