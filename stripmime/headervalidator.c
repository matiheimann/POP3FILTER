#include <stdlib.h>
#include <ctype.h>

#include "headervalidator.h"

headervalidator* initheadervalidator()
{
	headervalidator* hv = malloc(sizeof(headervalidator));
	hv->headers = malloc(sizeof(char*) * 3);
	hv->headers[0] = "content-type";
	hv->headers[1] = "mime-version";
	hv->headers[2] = "content-transfer-enconding";
	hv->isvalid = malloc(sizeof(int) * 3);
	hv->stillvalid = 1;
	hv->matchfound = 0;
	hv->index = 0;
	return hv;
}

int checkheader(headervalidator* hv, char c)
{
	hv->stillvalid = 0;
	for(int i = 0; i < 3; i++)
	{
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

	return hv->stillvalid;
}

void destroyheadervalidator(headervalidator* hv)
{
	free(hv->headers);
	free(hv->isvalid);
	free(hv);
}