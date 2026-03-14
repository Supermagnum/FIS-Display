#include "fis_config.h"
#include "fis_config_defaults.h"

void fis_config_set_defaults(fis_config_t *c) {
    if (!c) return;
    c->show_clock   = (FIS_SHOW_CLOCK_DEFAULT != 0);
    c->show_eta     = (FIS_SHOW_ETA_DEFAULT != 0);
    c->show_compass = (FIS_SHOW_COMPASS_DEFAULT != 0);
    c->show_remain  = (FIS_SHOW_REMAIN_DEFAULT != 0);
    c->can_enabled  = (FIS_CAN_ENABLED_DEFAULT != 0);
}
