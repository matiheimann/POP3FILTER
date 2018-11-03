#ifndef POP3FMP_H
#define POP3FMP_H

#include <stdint.h>
#include "buffer.h"

#define MAX_RESPONSE 250

#define MAX_USER 16
#define MAX_PASSWORD 16

enum POP3FMP_REQUEST_STATES {
    START,
    CLOSE_CONNECTION,
    /** la respuesta esta completa*/
    END,
    /** get */
    GET,
    /** set */
    SET,
    /** user */
    USER,
    /** pass */
    PASS,
    /** set mediatypes */
    SET_MEDIATYPES,
    /** set message */
    SET_MESSAGE,
    /** set filter */
    SET_FILTER,
    /** set mediatypes end*/
    END_SET_MEDIATYPES,
    /** set message end*/
    END_SET_MESSAGE,
    /** set filter */
    END_SET_FILTER,
    /** version get set */
    VERSION_GET_SET,
    /** version auth */
    VERSION_AUTH,
    /** bad request */
    END_BAD_REQUEST,
};


uint8_t* receivePOP3FMPRequest(buffer* b, int* size);
int transitions(uint8_t feed, int* state, uint8_t* response, int * size, char* str, int* strIndex);

#endif