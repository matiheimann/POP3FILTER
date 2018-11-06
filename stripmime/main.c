#include <stdlib.h>
#include <stdio.h>
#include <string.h>

#include "mediatypesfilter.h"

int main(int argc, char  *argv[])
{
	
	char* filterMessage = getenv("FILTER_MSG");
	char* censoredMediaTypes = getenv("FILTER_MEDIAS");


	if(filterMessage == NULL)
	{
		exit(1);
	}

	if(censoredMediaTypes == NULL)
	{
		exit(1);
	}

	filteremail(censoredMediaTypes, filterMessage);

	return 0;
}

