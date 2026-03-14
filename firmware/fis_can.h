#ifndef FIS_CAN_H
#define FIS_CAN_H

#include "fis_config.h"

/* CAN bus support is optional and disabled by default (fis_config_defaults.h:
 * FIS_CAN_ENABLED_DEFAULT 0). The Pico 2 W has no on-chip CAN; use MCP2515 (SPI)
 * plus MCP2551 transceiver when implementing. B6 comfort/infotainment CAN runs
 * at 100 kbit/s; set MCP2515 bit timing accordingly. This module is a stub until
 * hardware support is added. */

void fis_can_poll(const fis_config_t *config);

#endif
