#include <string.h>
#include <stdlib.h>
#include <ctype.h>
#include <stdio.h>

#include "mediatypestree.h"

mediatypetree* initializeTree()
{
	mediatypetree* tree = malloc(sizeof(mediatypetree));
	tree->root = malloc(sizeof(treenode));
	tree->root->children = NULL;
	tree->root->wildcard = 0;
	tree->root->value = "root";
	tree->root->sons = 0;
	return tree;
}

treenodeADT addMediaTypeToTree(treenodeADT n, char* mediatype)
{
	if(mediatype[0] == 0 || mediatype[0] == '*')
	{
		n->wildcard = 1;
		n->children=NULL;
		n->sons = 0;
		return n;
	}
	int length = getTypeLength(mediatype);
	if(n->children == NULL)
	{
		n->children = malloc(sizeof(treenodeADT));
		n->children[0] = malloc(sizeof(treenode));
		n->children[0]->value = malloc((length + 1)*sizeof(char));
		strncpy(n->children[0]->value,mediatype, length);
		n->sons = 1;
		if(mediatype[length] != 0)
		{
			n->children[0]->sons = 1;
			n->children[0]->wildcard = 0;
			n->children[0] = addMediaTypeToTree(n->children[0], mediatype + length + 1);
			return n;
		}
		else
		{
			n->children[0]->sons = 0;
			n->children[0]->wildcard = 1;
			return n;
		}
	}
	else
	{
		for(int i = 0; i < n->sons; i++)
		{
			if(strncmp(mediatype, n->children[i]->value, length) == 0
				&& strlen(n->children[i]->value) == (unsigned int)length)
			{
				n->children[i] = addMediaTypeToTree(n->children[i], mediatype + length + 1);
				return n;
			}
		}
		n->children = realloc(n->children, sizeof(treenodeADT) * (n->sons + 1));
		n->children[n->sons] = malloc(sizeof(treenode));
		n->children[n->sons]->value = malloc((length + 1) * sizeof(char));
		n->children[n->sons]->children = NULL;
		strncpy(n->children[n->sons]->value, mediatype, length);
		n->sons += 1;
		if(mediatype[length] != 0)
		{
			n->children[n->sons - 1]->wildcard = 0;
			n->children[n->sons - 1]->sons = 1;
			n->children[n->sons - 1] = addMediaTypeToTree(n->children[n->sons - 1], mediatype + length + 1);
			return n;
		}
		else
		{
			n->children[n->sons - 1]->wildcard = 1;
			n->children[n->sons - 1]->sons = 0;
			return n;
		}
	}
}

void printTree(treenode* n, int size)
{
	if(n == NULL)
	{
		return;
	}

	for(int i = 0; i < size; i++)
	{
		printf("-");
	}
	printf("%s with %d sons\n", n->value, n->sons);

	if(n->wildcard)
	{
		return;
	}

	if(n->children == NULL)
	{
		return;
	}

	for(int i = 0; i < n->sons; i++)
	{
		printTree(n->children[i], size + 1);
	}
}

int getTypeLength(char* mt)
{
	int i = 0;
	while(isalnum(mt[i]))
	{
		i++;
	}
	return i;
}