#include <stdlib.h>

#include "stackint.h"

stackint* initIntStack()
{
	stackint* s = malloc(sizeof(stackint));
	s->peek = NULL;
	return s;
}

int peekInt(stackint* s)
{
	return (s->peek == NULL) ? -1 : s->peek->value;
}

int popInt(stackint* s)
{
	if(s->peek == NULL)
		return -1;

	stackintnodeADT aux = s->peek;
	s->peek = s->peek->next;
	return aux->value;
}

void pushInt(stackint* s, int value)
{
	stackintnodeADT aux = malloc(sizeof(stackintnodeADT));
	aux->next = s->peek;
	aux->value = value;
	s->peek = aux;
}