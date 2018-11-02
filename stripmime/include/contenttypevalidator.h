#ifndef CONTENT_TYPE_VALIDATOR_H
#define CONTENT_TYPE_VALIDATOR_H 

typedef struct contentypevalidator
{
	int quantityMediaTypes;
	int stillValid;
	char* mediatypes;
	int* isValid;
	int* startingIndex;
	int ignore;
	int index;
	int matchfound;
	int lastmatch;
	
}contentypevalidator;

contentypevalidator* initcontenttypevalidator(char* mts);
void destroycontenttypevalidator(contentypevalidator* ctp);
int checkmediatypes(contentypevalidator* ctp, char c);

#endif