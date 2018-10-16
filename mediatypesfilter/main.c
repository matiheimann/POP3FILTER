#include <stdlib.h>
#include <stdio.h>
#include <string.h>

#include "mediatypestree.h"
#include "main.h"

int main(int argc, char  *argv[])
{
	
	//char* filterMessage = getenv("FILTER_MSG");
	//char* censoredMediaTypes = getenv("FILTER_MEDIAS");

	if(filterMessage == NULL)
	{
		fprintf(stderror, "It wasn't possible to set the replacement message\n");
		exit(1);
	}

	if(censoredMediaTypes == NULL)
	{
		fprintf(stderror, "It wasn't possible to set the media types to censor\n");
	}

	mediatypetree* tree = initializeTree();
	//addMediaTypesToTree(censoredMediaTypes, tree);
	addMediaTypesToTree(argv[1], tree);
	return 0;
}

void addMediaTypesToTree(char* mediatypes, mediatypetree* tree)
{
	int i = 0;
	while(mediatypes[i] != 0)
	{
		char* mt = getNextMediaType(&i, mediatypes);
		tree->root=addMediaTypeToTree(tree->root, mt);
	}
}

char* getNextMediaType(int* index, char* mediatypes)
{
	int i = *(index);
	int j = *(index);
	while(mediatypes[i] != 0 && mediatypes[i] != ',')
	{
		i++;
	}
	
	char* ret = malloc(sizeof(char) * (i - j + 1));
	strncpy(ret, mediatypes + j, i - j);
	*(index) = (mediatypes[i] != 0) ? i+1 : i;
	return ret;
}