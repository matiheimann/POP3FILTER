#include <stdlib.h>
#include <ctype.h>
#include <stdio.h>

#include "contenttypevalidator.h"

contentypevalidator* initcontenttypevalidator(char* mts)
{
	contentypevalidator* ctp = malloc(sizeof(contentypevalidator));
	ctp->mediatypes = mts;
	
	int i = 0;
	if(mts[0] == 0)
	{
		ctp->quantityMediaTypes = 0;
		ctp->isValid = NULL;
		ctp->startingIndex = NULL;
		ctp->stillValid = 0;
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

	ctp->stillValid = 1;
	ctp->quantityMediaTypes = counter;
	ctp->isValid = malloc(counter * sizeof(int));
	ctp->startingIndex = malloc(counter * sizeof(int));
	ctp->ignore = 0;
	ctp->index = 0;
	ctp->matchfound = 0;

	for(i = 0; i < counter; i++)
	{
		ctp->isValid[i] = 1;
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
	free(ctp->isValid);
	free(ctp->startingIndex);
	free(ctp);
	return;
}

int checkmediatypes(contentypevalidator* ctp, char c)
{

	if(ctp->ignore)
	{
		if(c == ')')
		{
			ctp->ignore = 0;
		}
		return 0;
	}

	if(c == '(')
	{
		ctp->ignore = 1;
	}
	else if(c == ' ')
	{
		return 0;
	}

	ctp->stillValid = 0;
	for(int i = 0; i < ctp->quantityMediaTypes; i++)
	{
		if(ctp->isValid[i])
		{
			int carachter = ctp->startingIndex[i] + ctp->index;
			if(ctp->mediatypes[carachter] == '*')
			{
				ctp->matchfound = 1;
				return 1;
			}
			else if(c == ';' && (ctp->mediatypes[carachter] == 0 ||
				ctp->mediatypes[carachter] == ','))
			{
				ctp->matchfound = 1;
				return 1;
			}
			if(ctp->mediatypes[carachter] == ',' || ctp->mediatypes[carachter] == 0 ||
				tolower(ctp->mediatypes[carachter]) != tolower(c))
			{
				ctp->isValid[i] = 0;
			}
			else
			{
				ctp->stillValid = 1;
			}
		}
	}

	(ctp->index)++;

	return ctp->stillValid;
}
