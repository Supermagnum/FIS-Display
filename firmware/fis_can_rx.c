/*
 * K-CAN receive parsing: extract signals from cluster/vehicle messages.
 * Layouts per README section 6.4 (PQ35/46 CAN matrix); bytes 1-based in spec, 0-based here.
 */

#include "fis_can_rx.h"
#include "fis_can.h"
#include <string.h>

void fis_can_rx_state_init(fis_can_rx_state_t *state) {
    if (!state)
        return;
    memset(state, 0, sizeof(*state));
    state->speed_kmh_x100 = FIS_CAN_RX_SPEED_INVALID;
    state->outside_temp_c = FIS_CAN_RX_TEMP_INVALID;
    state->standzeit_4s = FIS_CAN_RX_STANDZEIT_INVALID;
}

/* Extract num_bits from data starting at data[first_byte], bit first_bit (0..7). LSB first. */
static uint32_t get_bits(const uint8_t *data, unsigned int first_byte, unsigned int first_bit, unsigned int num_bits) {
    uint32_t res = 0u;
    for (unsigned int i = 0; i < num_bits; i++) {
        unsigned int b = first_byte + (first_bit + i) / 8u;
        unsigned int bit = (first_bit + i) % 8u;
        if (data[b] & (1u << bit))
            res |= (1u << i);
    }
    return res;
}

static bool get_bit(const uint8_t *data, unsigned int byte_idx, unsigned int bit_idx) {
    return (data[byte_idx] & (1u << bit_idx)) != 0u;
}

void fis_can_rx_parse(const fis_can_frame_t *frame, fis_can_rx_state_t *state) {
    if (!frame || !state || frame->dlc < 2u)
        return;

    const uint8_t *d = frame->data;
    const uint16_t id = frame->id & FIS_CAN_STD_ID_MASK;

    switch (id) {
    case FIS_CAN_ID_MKOMBI_1:
        if (frame->dlc >= 8u) {
            state->speed_kmh_x100 = (uint16_t)get_bits(d, 3, 1, 15);
            state->fuel_litres = (uint8_t)(get_bits(d, 2, 0, 7) & 0x7Fu);
            state->fuel_low_warning = get_bit(d, 0, 6);
            state->oil_pressure_warning = get_bit(d, 0, 2);
            state->coolant_warning = get_bit(d, 0, 4);
            state->glow_plug_on = get_bit(d, 0, 7);
        }
        break;

    case FIS_CAN_ID_MKOMBI_2:
        if (frame->dlc >= 6u) {
            uint8_t raw_temp = (uint8_t)get_bits(d, 2, 0, 8);
            state->outside_temp_c = (raw_temp == 0xFFu) ? FIS_CAN_RX_TEMP_INVALID : (int8_t)((int)(raw_temp * 5) / 10 - 50);
            state->display_brightness_pct = (uint8_t)(get_bits(d, 5, 0, 7) & 0x7Fu);
            if (state->display_brightness_pct > 100u)
                state->display_brightness_pct = 100u;
        }
        break;

    case FIS_CAN_ID_MKOMBI_3:
        if (frame->dlc >= 8u) {
            state->odometer_km = get_bits(d, 5, 0, 20);
            state->standzeit_4s = get_bits(d, 3, 0, 15);
        }
        break;

    case FIS_CAN_ID_MGATE_KOMF_1:
        if (frame->dlc >= 8u) {
            state->reverse = get_bit(d, 3, 4);
            state->hazards_on = get_bit(d, 6, 7);
            state->door_driver = get_bit(d, 2, 0);
            state->door_passenger = get_bit(d, 5, 1);
            state->door_rear_left = get_bit(d, 3, 2);
            state->door_rear_right = get_bit(d, 3, 3);
            state->wipers_front = get_bit(d, 6, 2);
        }
        break;

    case FIS_CAN_ID_MZAS_1:
        if (frame->dlc >= 2u) {
            state->key_inserted = get_bit(d, 0, 0);
            state->ignition_on = get_bit(d, 0, 1);
            state->starter_active = get_bit(d, 0, 3);
        }
        break;

    case FIS_CAN_ID_PARKHILFE_01:
        if (frame->dlc >= 8u) {
            state->pdc_obstacle_front = get_bit(d, 2, 2);
            state->pdc_obstacle_rear = get_bit(d, 2, 3);
            state->pdc_system_state = (uint8_t)(get_bits(d, 7, 2, 3) & 0x07u);
        }
        break;

    default:
        break;
    }
}

void fis_can_rx_poll(const fis_config_t *config, fis_can_rx_state_t *state) {
    if (!config || !config->can_enabled || !state)
        return;

    fis_can_ensure_init();
    fis_can_frame_t frame;
    while (fis_can_receive(&frame)) {
        fis_can_rx_parse(&frame, state);
    }
}
