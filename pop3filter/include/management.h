#ifndef MANAGEMENT_H
#define MANAGEMENT_H

#include "selector.h"

/** handler del socket pasivo que atiende conexiones management */
void
management_passive_accept(struct selector_key *key);

enum AUTH_STATES {
    /** management client is logged out */
    LOGGED_OUT,
    /** has acceptable username */
    USER_ACCEPTED,
    /** has acceptable username */
    PASS_ACCEPTED
};

#endif