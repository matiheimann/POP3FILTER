#ifndef MAIN_FUNCTIONS_H
#define MAIN_FUNCTIONS_H

#include "mediatypestree.h"

void addMediaTypesToTree(char* mediatypes, mediatypetree* tree);
char* getNextMediaType(int* index, char* mediatypes);

#endif