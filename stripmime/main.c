#include <stdlib.h>
#include <stdio.h>
#include <string.h>

#include "mediatypesfilter.h"
#include "mainfunctions.h"

int main(int argc, char  *argv[])
{
	
	char* filterMessage = getenv("FILTER_MSG");
	char* censoredMediaTypes = getenv("FILTER_MEDIAS");

	if(filterMessage == NULL)
	{
		//fprintf(stderror, "It wasn't possible to set the replacement message\n");
		exit(1);
	}

	if(censoredMediaTypes == NULL)
	{
		//fprintf(stderror, "It wasn't possible to set the media types to censor\n");
		exit(1);
	}

	mediatypetree* tree = initializeTree();
	//addMediaTypesToTree(censoredMediaTypes, tree);
	addMediaTypesToTree(argv[1], tree);

	filteremail(tree, filterMessage);

	return 0;
}



