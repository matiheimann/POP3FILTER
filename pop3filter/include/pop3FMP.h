#ifndef POP3FMP_H
#define POP3FMP_H

#define MAX_RESPONSE 250

#define MAX_USER 16
#define MAX_PASSWORD 16

enum POP3FMP_REQUEST_STATES {
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


uint8_t* receivepop3FMP(buffer* b, int* size);

#endif