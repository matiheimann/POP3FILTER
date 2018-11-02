#include <stdlib.h>

#include "stackstring.h"

stackstring* initStringStack()
{
	stackstring* s = malloc(sizeof(stackstring));
	s->peek = NULL;
	return s;
}

char* peekString(stackstring* s)
{
	return (s->peek == NULL) ? NULL : s->peek->value;
}

char* popString(stackstring* s)
{
	if(s->peek == NULL)
		return NULL;

	stackstringnodeADT aux = s->peek;
	s->peek = s->peek->next;
	return aux->value;
}

void pushString(stackstring* s, char* value)
{
	stackstringnodeADT aux = malloc(sizeof(stackstringnodeADT));
	aux->next = s->peek;
	aux->value = value;
	s->peek = aux;
}