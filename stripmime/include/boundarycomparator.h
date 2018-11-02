#ifndef BOUNDARY_COMPARATOR_H
#define BOUNDARY_COMPARATOR_H

typedef struct boundarycomparator
{
	char* boundary;
	int boundarylength;
	int index;
	int stillvalid;
	int endingboundary;
	int match;

}boundarycomparator;

boundarycomparator* initboundarycomparator(char* boundary);
int compareboundaries(boundarycomparator* bc, char c);
void destroyboundarycomparator(boundarycomparator* bc);

#endif