#ifndef OPTIONS_H
#define OPTIONS_H

#include <stdint.h>

typedef struct {
    int managementPort;
    struct sockaddr_in6 managementAddress;
} options_st;

extern options_st * options;


void setConfiguration(int argc, char* const argv[]);
void setManagementAddress(char* dir);
void setManagementPort(char* port);
int isANumericArgument(char* value, char param);
void printHelp();

#endif