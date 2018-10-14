#ifndef MANAGEMENT_H
#define MANAGEMENT_H

#include "selector.h"

/** handler del socket pasivo que atiende conexiones management */
void
management_passive_accept(struct selector_key *key);

#endif