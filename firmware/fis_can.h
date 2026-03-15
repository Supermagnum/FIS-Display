#ifndef FIS_CAN_H
#define FIS_CAN_H

#include "fis_config.h"
#include <stdbool.h>
#include <stdint.h>

/* CAN bus support is optional and disabled by default (fis_config_defaults.h:
 * FIS_CAN_ENABLED_DEFAULT 0). The Pico 2 W has no on-chip CAN; the CAN
 * transceiver is the MCP2561. It has only 2 signal connections to the Pico:
 * TXD and RXD. B6 comfort/infotainment CAN runs at 100 kbit/s.
 * This module implements software CAN 2.0A (11-bit ID) at 100 kbit/s. */

/* Pins for MCP2561 (3.3 V logic). Only TXD and RXD; no SPI. */
#define FIS_CAN_PIN_TX  11u   /* Pico GPIO 11 -> MCP2561 TXD (TX CAN) */
#define FIS_CAN_PIN_RX  12u   /* MCP2561 RXD -> Pico GPIO 12 (RX CAN) */

#define FIS_CAN_MAX_DLC  8u
#define FIS_CAN_STD_ID_MASK  0x7FFu

/* Standard 11-bit ID CAN 2.0A frame. */
typedef struct {
    uint16_t id;       /* 11-bit identifier (FIS_CAN_STD_ID_MASK) */
    bool rtr;          /* remote transmission request */
    uint8_t dlc;       /* data length 0..8 */
    uint8_t data[FIS_CAN_MAX_DLC];
} fis_can_frame_t;

/* Initialize CAN GPIO and timing. Safe to call multiple times; no-op if already init. */
void fis_can_init(void);

/* Send one standard frame. Returns true on success. Blocks for the duration of the frame. */
bool fis_can_send(const fis_can_frame_t *frame);

/* Poll for a received frame. Returns true if *out was filled with a valid frame. Non-blocking. */
bool fis_can_receive(fis_can_frame_t *out);

/* Called from main loop when CAN is enabled; runs init if needed and drains RX. */
void fis_can_poll(const fis_config_t *config);

#endif
