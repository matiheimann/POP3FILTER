#ifndef MEDIA_TYPES_FILTER_H
#define MEDIA_TYPES_FILTER_H

#include "mediatypestree.h"

typedef enum states
{
	END,
	NEW_LINE_HEADER,
	CHECKING_HEADERS,
	CHECK_MEDIA_TYPE,
	NEW_LINE_BODY,
	BODY,
	LINE_ONLY_DOT

}states;

void filteremail(mediatypetree* tree, char* filterMessage);
int isContentTypeHeader(char* header);

#endif