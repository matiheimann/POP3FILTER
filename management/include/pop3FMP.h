#ifndef POP3FMP_H
#define POP3FMP_H

#include <stdint.h>
#include "buffer.h"

enum POP3FMP_RESPONSE_STATES {
    START,
    ERROR,
    /** la respuesta esta completa*/
    END,
    /** la respuesta es a version no soportada*/
    VERSION_NOT_SUPPORTED,
    /** bad request*/
    VERSION_BAD_REQUEST,
    /** version metric */
    VERSION_METRIC,
    /** version get set */
    VERSION_GET_SET,
    /** version auth */
    VERSION_AUTH,
    /** string */
    STRING,
};

void transitions(uint8_t feed, int* state, char* str, int* strIndex);
int receivePOP3FMPResponse(buffer * b);
void consumeBuffer(buffer* b);

#endif
