#include <stdlib.h>
#include <ctype.h>

#include "boundaryvalidator.h"

boundaryvalidator* initboundaryvalidator()
{
	boundaryvalidator* bv = malloc(sizeof(boundaryvalidator));
	bv->index = 0;
	bv->stillvalid = 1;
	bv->boundaryfound = 0;
	bv->boundary = NULL;
	bv->end = 0;
	return bv;
}

void destroyboundaryvalidator(boundaryvalidator* bv)
{
	free(bv->boundary);
	free(bv);
}

int checkboundary(boundaryvalidator* bv, char c)
{

	if(c == ' ' && bv->boundary == NULL)
		return 0;

	if(!bv->boundaryfound)
	{
		char* aux = "boundary";
		if(c == '=' && aux[bv->index] == 0)
		{
			bv->boundaryfound = 1;
		}
		else if(tolower(c) != aux[bv->index])
		{
			bv->stillvalid = 0;
			return 0;
		}

		(bv->index)++;

		return 1;
	}


	if(c == '"')
	{
		if(bv->boundary == NULL)
		{
			bv->boundary = calloc(70, sizeof(char));
			bv->index = 0;
			return 1;
		}
		else 
		{
			bv->boundary = realloc(bv->boundary, sizeof(char) * (bv->index + 1));
			bv->end = 1;
			return 1;
		}
	}

	bv->boundary[bv->index] = c; 
	(bv->index)++;

	return 1;

}