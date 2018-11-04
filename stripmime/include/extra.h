#ifndef EXTRA_H
#define EXTRA_H

typedef struct extrainformation
{
	int size;
	int blocksize;
	char* buff;

}extrainformation;

extrainformation* initextrainformation(int b);
void addchar(extrainformation* e, char c);
void endextrainformation(extrainformation* e);
void destroyextrainformation(extrainformation* e);

#endif