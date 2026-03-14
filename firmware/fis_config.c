#include "fis_config.h"

void fis_config_set_defaults(fis_config_t *c) {
    if (!c) return;
    c->show_clock   = true;
    c->show_eta     = true;
    c->show_compass = true;
    c->show_remain  = true;
}
