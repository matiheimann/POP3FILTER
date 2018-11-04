#ifndef OPTIONS_H
#define OPTIONS_H

#include <stdint.h>

typedef struct {
    uint32_t pop3Direction;
    uint32_t managementDirection;
    int managementPort;
} options_st;

extern options_st * options;


void setConfiguration(int argc, char* const argv[]);
void setManagementDirection(char* dir);
void setManagementPort(char* port);
int isANumericArgument(char* value, char param);
void printHelp();

#endif