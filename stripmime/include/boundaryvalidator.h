#ifndef BOUNDARY_VALIDATOR_H
#define BOUNDARY_VALIDATOR_H

typedef struct boundaryvalidator
{

	int index;
	char* boundary;
	int boundaryfound;
	int stillvalid;
	int end;


}boundaryvalidator;

boundaryvalidator* initboundaryvalidator();
void destroyboundaryvalidator(boundaryvalidator* bv);
int checkboundary(boundaryvalidator* bv, char c);

#endif