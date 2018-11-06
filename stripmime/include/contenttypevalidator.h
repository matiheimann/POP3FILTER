#ifndef CONTENT_TYPE_VALIDATOR_H
#define CONTENT_TYPE_VALIDATOR_H

typedef enum typesofmatches
{
	NORMAL_MATCH = 1,
	MULTIPART_MATCH,
	MESSAGE_MATCH

}typesofmatches;

typedef struct contentypevalidator
{
	int quantityMediaTypes;
	/*Flag si es de los censurables*/
	int stillValidCensored;
	/*Flag si es multipart o message*/
	int stillValidExtras;
	/*Media types censurables separados por ,*/
	char* mediatypes;
	/*Para message/ y multipart/ que si no estan censurados tienen
	un comportamiento diferente al de todos los media types*/
	char** extramediatypes;
	int* isValidCensored;
	int* isValidExtras;
	int* startingIndex;
	/*Flag de comentario*/
	int ignore;
	int index;
	int matchfound;
	int lastmatch;
	
}contentypevalidator;

contentypevalidator* initcontenttypevalidator(char* mts);
void destroycontenttypevalidator(contentypevalidator* ctp);
int checkmediatypes(contentypevalidator* ctp, char c);

#endif