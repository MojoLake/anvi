#ifndef SESSION_SETUP_H
#define SESSION_SETUP_H

#include "app.h"

int setup_initial_state(struct coldwrite_state *state);
void destroy_coldwrite_state(struct coldwrite_state *state);

#endif
