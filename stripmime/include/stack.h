#ifndef STACK_H
#define STACK_H

typedef struct stacknode* stacknodeADT;

typedef struct stacknode
{
	
	char* value;
	stacknodeADT next;

} stacknode; 

typedef struct stack
{
	
	stacknodeADT peek;

}stack;

stack* initStack();
char* peek(stack* elem);
char* pop(stack* node);
void push(stack* s, char* value);

#endif