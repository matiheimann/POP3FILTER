#ifndef OPTIONS_H
#define OPTIONS_H

#include <stdint.h>

typedef struct {
    char* originServer;
    char* errorFile;
    struct sockaddr_in6 pop3Address;
    struct sockaddr_in6 managementAddress;
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
void setPop3Address(char* dir);
void setManagementAddress(char* dir);
int isANumericArgument(char* dir, char param);
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