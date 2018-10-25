#ifndef MANAGEMENT_PARSER_H
#define MANAGEMENT_PARSER_H

unsigned char* readCommand(int* size);
int checkValidMediaTypes(char * str);
int startsWith(const char* str, const char* start);
void helpMessage();

#endif
