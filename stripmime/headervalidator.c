#include <stdlib.h>
#include <ctype.h>

#include "headervalidator.h"

headervalidator* initheadervalidator()
{
	headervalidator* hv = malloc(sizeof(headervalidator));
	hv->headers = malloc(sizeof(char*) * 4);
	hv->headers[0] = "content-type";
	hv->headers[1] = "content-transfer-enconding";
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
	hv->stillvalid = 0;
	for(int i = 0; i < 4; i++)
	{
		hv->lastmatch = i;
		if(hv->isvalid[i])
		{
			if(c == ':' && hv->headers[i][hv->index] == 0)
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