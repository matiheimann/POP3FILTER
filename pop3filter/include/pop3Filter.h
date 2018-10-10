#ifndef POP3_FILTER_H
#define POP3_FILTER_H

#include "selector.h"

/** handler del socket pasivo que atiende conexiones pop3filter */
void
pop3filter_passive_accept(struct selector_key *key);


/** libera pools internos */
void
pop3filter_pool_destroy(void);

#endif