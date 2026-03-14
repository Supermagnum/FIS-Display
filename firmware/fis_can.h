#ifndef FIS_CAN_H
#define FIS_CAN_H

#include "fis_config.h"

/* CAN bus support is optional and disabled by default (fis_config_defaults.h:
 * FIS_CAN_ENABLED_DEFAULT 0). The Pico 2 W has no on-chip CAN; the CAN
 * controller used is the MCP2561 transceiver. It has only 2 signal connections
 * to the Pico: TXD and RXD. B6 comfort/infotainment CAN runs at 100 kbit/s.
 * This module is a stub until hardware support is added. */

/* Pins for MCP2561 (3.3 V logic). Only TXD and RXD; no SPI. */
#define FIS_CAN_PIN_TX  11u   /* Pico GPIO 11 -> MCP2561 TXD (TX CAN) */
#define FIS_CAN_PIN_RX  12u   /* MCP2561 RXD -> Pico GPIO 12 (RX CAN) */

void fis_can_poll(const fis_config_t *config);

#endif
