#ifndef MEDIA_TYPES_FILTER_H
#define MEDIA_TYPES_FILTER_H

#include "contenttypevalidator.h"
#include "headervalidator.h"
#include "stack.h"
#include "boundaryvalidator.h"

typedef enum states
{
	NEW_LINE,
	CHECKING_HEADER,
	CHECKING_CONTENT_TYPE,
	CHECKING_BOUNDARY,
	CHECKING_BODY
}states;

typedef struct ctx
{
	int action;
	int lastAction;
	int mimetypedeclared;
	int contenttypedeclared;
	contentypevalidator* ctp;
	headervalidator* hv;
	boundaryvalidator* bv;
	stack* censored;
	stack* boundaries;

}ctx;

void filteremail(char* cesoredMediaTypes, char* filterMessage);
ctx* initcontext();
void destroycontext(ctx* context);

#endif