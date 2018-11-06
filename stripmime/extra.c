#include <stdlib.h>

#include "extra.h"

extrainformation* initextrainformation(int b)
{
	extrainformation* e = malloc(sizeof(extrainformation));
	e->blocksize = b;
	e->size = 0;
	e->buff = NULL;	
	return e;
}
void addchar(extrainformation* e, char c)
{
	if(e->size % e->blocksize == 0)
	{
		e->buff = realloc(e->buff, (e->size + e->blocksize));
	}
	e->buff[e->size] = c;
	e->size++;
}
void endextrainformation(extrainformation* e)
{
	e->buff = realloc(e->buff, e->size);
}
void destroyextrainformation(extrainformation* e)
{
	free(e->buff);
	free(e);
}