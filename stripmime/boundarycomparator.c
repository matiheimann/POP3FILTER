#include <stdlib.h>
#include <string.h>

#include "boundarycomparator.h"

boundarycomparator* initboundarycomparator(char* boundary)
{
	boundarycomparator* bc = malloc(sizeof(boundarycomparator));
	bc->boundary = boundary;
	bc->boundarylength = strlen(bc->boundary);
	bc->index = -2;
	bc->stillvalid = 1;
	bc->endingboundary = 0;
	bc->match = 0;
	return bc;
}
int compareboundaries(boundarycomparator* bc, char c)
{

	/*Se verifica la validez del boundary planteado*/

	if(c == '\r')
	{
		if(!bc->match)
		{
			bc->stillvalid = 0;
		}
		return 0;
	}

	/*Se verifica la presencia de ambos guiones*/

	if(bc->index < 0)
	{
		if(c != '-')
		{
			bc->stillvalid = 0;
		}
		else
		{
			bc->index++;
		}
		return 1;
	}

	/*Compara el valor del boundary*/

	else if(bc->boundarylength <= bc->index)
	{
		bc->match = 0;
		if(bc->index >= bc->boundarylength + 2)
		{
			bc->match = 0;
			bc->endingboundary = 0;
			bc->stillvalid = 0;
			return 0;
		}
		else if(c != '-')
		{
			bc->stillvalid = 0;
			return 0;
		}
		else if(bc->index == bc->boundarylength + 1)
		{
			bc->match = 1;
			bc->endingboundary = 1;
		}
	}

	/*Verifica los dos guiones de cierre en el caso que sea el boundary de cierre*/

	else
	{
		if(bc->boundary[bc->index] != c)
		{
			bc->stillvalid = 0;
			return 0;
		}
		if(bc->boundarylength - 1 == bc->index)
		{
			bc->match = 1;
		}
	}
	(bc->index)++;

	return 1;
}

void destroyboundarycomparator(boundarycomparator* bc)
{
	free(bc);
}