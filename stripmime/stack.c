#include <stdlib.h>

#include "stack.h"

stack* initStack()
{
	stack* s = malloc(sizeof(stack));
	s->peek = NULL;
	return s;
}

char* peek(stack* s)
{
	return (s->peek == NULL) ? NULL : s->peek->value;
}

char* pop(stack* s)
{
	if(s->peek == NULL)
		return NULL;

	stacknodeADT aux = s->peek;
	s->peek = s->peek->next;
	return aux->value;
}

void push(stack* s, char* value)
{
	stacknodeADT aux = malloc(sizeof(stacknodeADT));
	aux->next = s->peek;
	s->peek = aux;
}