#ifndef HEADER_VALIDATOR_H
#define HEADER_VALIDATOR_H

typedef enum matches
{
	CONTENT_TYPE = 1,
	MIME_VERSION,
	CONTENT_TRANSFER_ENCONDING
}matches;

typedef struct headervalidator
{
	
	char** headers;
	int index;
	int* isvalid;
	int stillvalid;
	int matchfound;

}headervalidator;

headervalidator* initheadervalidator();
int checkheader(headervalidator* hv, char c);
void destroyheadervalidator(headervalidator* hv);

#endif