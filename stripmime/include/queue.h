#ifndef QUEUE_H
#define QUEUE_H

typedef struct queuenode* queuenodeADT;

typedef struct queuenode
{
	
	char* value;
	queuenodeADT next;

} queuenode; 

typedef struct queue
{
	
	queuenodeADT first;
	queuenodeADT last;

}queue;

queue* initqueue();
char* first(queue* q);
char* dequeue(queue* q);
void enqueue(queue* q, char* value);

#endif