#ifndef OPTIONS_H
#define OPTIONS_H

#include <stdint.h>

typedef struct {
    char* originServer;
    char* errorFile;
    uint32_t pop3Direction;
    uint32_t managementDirection;
    char* replacementMessage;
    int selectedReplacementMessage;
    char* censoredMediaTypes;
    int managementPort;
    int localPort;
    int originPort;
    char* command;
    char* version;
} options_st;

extern options_st * options;

void setConfiguration(int argc, char* const argv[]);
void setOriginServer(char* server);
void setErrorFile(char* file);
void printHelp();
int isANumericArgument(char* dir, char param);
void setPop3Direction(char* dir);
void setManagementDirection(char* dir);
void setReplacementMessage(char* message);
void addCensoredMediaType(char* mediaType);
void setManagementPort(char* port);
void setLocalPort(char* port);
void setOriginPort(char* port);
void setCommand(char* cmd);
void printVersion();
char* strcatFixStrings(char* s1, char* s2);
void setFilterEnviromentVariables(char* mediaTypesCensored, char* replacementMessage);
int isValidFile(char* file);

#endif