#ifndef HEADER_VALIDATOR_H
#define HEADER_VALIDATOR_H

typedef enum matches
{
	CONTENT_TYPE = 1,
	CONTENT_TRANSFER_ENCONDING, 
	CONTENT_LENGTH,
	CONTENT_MD5
}matches;

typedef struct headervalidator
{
	
	char** headers;
	int index;
	int* isvalid;
	int stillvalid;
	int matchfound;
	int lastmatch;

}headervalidator;

headervalidator* initheadervalidator();
int checkheader(headervalidator* hv, char c);
void destroyheadervalidator(headervalidator* hv);

#endif