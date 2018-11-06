#include <stdlib.h>
#include <ctype.h>
#include <string.h>

#include "headervalidator.h"

headervalidator* initheadervalidator()
{
	headervalidator* hv = malloc(sizeof(headervalidator));
	hv->headers = malloc(sizeof(char*) * 4);
	hv->headers[0] = "content-type";
	hv->headers[1] = "content-transfer-encoding";
	hv->headers[2] = "content-length";
	hv->headers[3] = "content-md5";
	hv->isvalid = malloc(sizeof(int) * 4);
	for(int i = 0; i < 4; i++)
	{
		hv->isvalid[i] = 1;
	}
	hv->matchfound = 0;
	hv->index = 0;
	hv->stillvalid = 1;
	hv->lastmatch = 0;
	return hv;
}

int checkheader(headervalidator* hv, char c)
{
	/*Se verifica si los headers son los mismos, no son case-sensitive.
	Compara por indice y ante cualquier diferencia se desactiva el flag de
	validez del header*/
	hv->stillvalid = 0;
	for(int i = 0; i < 4; i++)
	{
		if(hv->isvalid[i])
		{
			hv->lastmatch = i;
			if(hv->index > (int)strlen(hv->headers[i]))
			{
				hv->isvalid[i] = 0;
			}
			else if(c == ':' && hv->headers[i][hv->index] == 0)
			{
				hv->matchfound = i+1;
				return 1;
			}
			else
			{
				if(tolower(c) == hv->headers[i][hv->index])
				{
					hv->stillvalid = 1;
				}
				else
				{
					hv->isvalid[i] = 0;
				}
			}
		}
	}
	
	if(hv->stillvalid)
		(hv->index)++;

	return hv->stillvalid;
}

void destroyheadervalidator(headervalidator* hv)
{
	free(hv->headers);
	free(hv->isvalid);
	free(hv);
}