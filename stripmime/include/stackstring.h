#ifndef STACK_STRING_H
#define STACK_STRING_H

typedef struct stackstringnode* stackstringnodeADT;

typedef struct stackstringnode
{
	
	char* value;
	stackstringnodeADT next;

} stackstringnode; 

typedef struct stackstring
{
	
	stackstringnodeADT peek;
	int size;

}stackstring;

stackstring* initStringStack();
char* peekString(stackstring* elem);
char* popString(stackstring* node);
void pushString(stackstring* s, char* value);
int isEmpty(stackstring* s);

#endif