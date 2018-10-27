#include <stdlib.h>

#include "queue.h"

queue* initqueue()
{
	queue* q = malloc(sizeof(queue));
	q->first = NULL;
	q->last = NULL;
	return q;
}

char* first(queue* q)
{
	if(q->first == NULL)
	{
		return NULL;
	}

	return q->first->value;
}

char* dequeue(queue* q)
{
	if(q->first == NULL)
	{
		return NULL;
	}

	char* ret = q->first->value;
	q->first = q->first->next;
	return ret;
}

void enqueue(queue* q, char* value)
{
	queuenode* node = malloc(sizeof(queuenode));
	node->next = NULL;
	if(q->first == NULL)
	{
		q->first = node;
		q->last = node;
		return;
	}
	q->last->next = node;
	q->last = node;
}