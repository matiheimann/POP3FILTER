#ifndef QUEUE_H
#define QUEUE_H

typedef struct queuenode* queuenodeADT;

typedef struct queuenode
{
	
	char* value;
	queuenodeADT next;

} stacknode; 

typedef struct queue
{
	
	queuenodeADT first;

}queue;

queue* initqueue();
char* first(queue* q);
char* dequeue(queue* q);
char* enqueue(queue* q, char* value);

#endif