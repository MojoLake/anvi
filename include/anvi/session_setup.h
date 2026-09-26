#ifndef SESSION_SETUP_H
#define SESSION_SETUP_H

#include <anvi/app.h>

struct anvi_config;

int setup_initial_state(struct anvi_state *state, struct anvi_config *config);
void destroy_anvi_state(struct anvi_state *state);

#endif
