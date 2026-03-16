/*
 * K-CAN (Komfort-CAN) receive parsing for VW Passat B6 (PQ46).
 * Parses cluster/vehicle messages and fills fis_can_rx_state_t for use by
 * display logic (e.g. MSP3222 TFT: speedo, temp, fuel, brightness, doors, PDC).
 * Message IDs and signal layouts per README section 6.4 / PQ35/46 CAN matrix.
 */

#ifndef FIS_CAN_RX_H
#define FIS_CAN_RX_H

#include "fis_can.h"
#include "fis_config.h"
#include <stdbool.h>
#include <stdint.h>

/* K-CAN message IDs (11-bit, from cluster/vehicle). */
#define FIS_CAN_ID_MKOMBI_1      0x320u   /* 10 ms: speed, fuel, warnings */
#define FIS_CAN_ID_MKOMBI_2      0x420u   /* 200 ms: temp, brightness */
#define FIS_CAN_ID_MKOMBI_3      0x520u   /* odometer, standzeit */
#define FIS_CAN_ID_MGATE_KOMF_1  0x390u   /* 100 ms: reverse, hazards, doors, wipers */
#define FIS_CAN_ID_MZAS_1        0x572u   /* ignition/key */
#define FIS_CAN_ID_PARKHILFE_01  0x497u   /* PDC obstacles */

/* Invalid / not received sentinels. */
#define FIS_CAN_RX_TEMP_INVALID    (-128)
#define FIS_CAN_RX_SPEED_INVALID   (0xFFFFu)
#define FIS_CAN_RX_STANDZEIT_INVALID (0xFFFFFFFFu)

/* Parsed K-CAN state (updated by fis_can_rx_parse). */
typedef struct {
    /* mKombi_1 (0x320) */
    uint16_t speed_kmh_x100;       /* 0-32600 = 0-326 km/h, FIS_CAN_RX_SPEED_INVALID if not received */
    uint8_t  fuel_litres;         /* 0-126 L */
    bool     fuel_low_warning;    /* KO1_Warn_Tank */
    bool     oil_pressure_warning;
    bool     coolant_warning;
    bool     glow_plug_on;

    /* mKombi_2 (0x420) */
    int8_t   outside_temp_c;      /* -50..+77, FIS_CAN_RX_TEMP_INVALID if invalid/not received */
    uint8_t  display_brightness_pct;  /* 0-100 % */

    /* mKombi_3 (0x520) */
    uint32_t odometer_km;
    uint32_t standzeit_4s;        /* time since ignition off, 4-second units; FIS_CAN_RX_STANDZEIT_INVALID if not received */

    /* mGate_Komf_1 (0x390) */
    bool     reverse;
    bool     hazards_on;
    bool     door_driver;
    bool     door_passenger;
    bool     door_rear_left;
    bool     door_rear_right;
    bool     wipers_front;

    /* mZAS_1 (0x572) */
    bool     key_inserted;        /* ZA1_S_Kontakt */
    bool     ignition_on;         /* ZA1_Klemme_15 */
    bool     starter_active;      /* ZA1_Klemme_50 */

    /* Parkhilfe_01 (0x497) */
    bool     pdc_obstacle_front;
    bool     pdc_obstacle_rear;
    uint8_t  pdc_system_state;    /* 0=off, 1=reverse, 2=front, 3=both, 7=fault */
} fis_can_rx_state_t;

/* Set state to defaults (invalids for speed/temp/standzeit, false for bools). */
void fis_can_rx_state_init(fis_can_rx_state_t *state);

/* Parse one received frame; update state if ID is known. */
void fis_can_rx_parse(const fis_can_frame_t *frame, fis_can_rx_state_t *state);

/* When CAN is enabled: init if needed, receive frames and parse into state. */
void fis_can_rx_poll(const fis_config_t *config, fis_can_rx_state_t *state);

#endif
