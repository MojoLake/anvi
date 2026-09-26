#ifndef CONFIG_H
#define CONFIG_H

#include <stdlib.h>

constexpr size_t MAX_FILE_PATH_LEN = 256;
struct anvi_config {
    char save_dir[MAX_FILE_PATH_LEN];
};

void anvi_config_set_defaults(struct anvi_config *config);
int anvi_config_load(struct anvi_config *config);


#endif
