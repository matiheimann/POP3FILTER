#ifndef OPTIONS_H
#define OPTIONS_H

#include <stdint.h>
#include <netinet/in.h>

typedef struct {
    char* originServer;

    struct addrinfo * originAddrInfo;

    char* errorFile;
    uint32_t pop3Direction;
    uint32_t managementDirection;
    char* replacementMessage;
    int selectedReplacementMessage;
    char** censurableMediaTypes;
    int censurableMediaTypesSize;
    int managementPort;
    int localPort;
    int originPort;
    char* command;
    char* version;
} options_st;

extern options_st * options;

void setConfiguration(int argc, char* const argv[]);
void setOriginServer(char* server);
void resolv_address(char *address, uint16_t port, struct addrinfo ** addrinfo);
void setErrorFile(char* file);
void printHelp();
int isANumericArgument(char* dir, char param);
void setPop3Direction(char* dir);
void setManagementDirection(char* dir);
void setReplacementMessage(char* message);
void addCensurableMediaType(char* mediaType);
void setManagementPort(char* port);
void setLocalPort(char* port);
void setOriginPort(char* port);
void setCommand(char* cmd);
void printVersion();

#endif