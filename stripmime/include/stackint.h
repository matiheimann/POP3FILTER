#ifndef STACK_INT_H
#define STACK_INT_H

typedef struct stackintnode* stackintnodeADT;

typedef struct stackintnode
{
	
	int value;
	stackintnodeADT next;

} stackintnode; 

typedef struct stackint
{
	
	stackintnodeADT peek;

}stackint;

stackint* initIntStack();
int peekInt(stackint* elem);
int popInt(stackint* node);
void pushInt(stackint* s, int value);


#endif