#ifndef MEDIA_TYPES_FILTER_H
#define MEDIA_TYPES_FILTER_H

#include "contenttypevalidator.h"

typedef enum states
{
	NEW_LINE,
	CHECKING_HEADER,
	CHECKING_CONTENT_TYPE,
	CHECKING_BODY
}states;

typedef struct ctx
{

	int action;
	int lastAction;
	contentypevalidator* ctp;
	stack* censored;
	stack* boundaries;

}ctx;

void filteremail(char* cesoredMediaTypes, char* filterMessage);
ctx* initcontext();

#endif