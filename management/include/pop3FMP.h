#ifndef POP3FMP_H
#define POP3FMP_H

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
} pop3FMPMessage;

void getData();

#endif
