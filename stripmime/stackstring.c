#include <stdlib.h>
#include <stdio.h>

#include "stackstring.h"

stackstring* initStringStack()
{
	stackstring* s = malloc(sizeof(stackstring));
	s->peek = NULL;
	s->size = 0;
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
	s->size--;
	return aux->value;
}

void pushString(stackstring* s, char* value)
{
	stackstringnodeADT aux = malloc(sizeof(stackstringnodeADT));
	aux->next = s->peek;
	aux->value = value;
	s->peek = aux;
	s->size++;
}
int isEmpty(stackstring* s)
{
	return (s->size == 0);
}