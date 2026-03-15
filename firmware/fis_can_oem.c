/*
 * PQ46/Komfort-CAN OEM message build and send.
 * mDiagnose_1 (0x7D0): date/time for cluster (from GPS when CAN enabled).
 * mEinheiten (0x60E): display format (24h, EU date, units).
 */

#include "fis_can_oem.h"
#include "fis_can.h"
#include "local_time.h"
#include "pico/time.h"
#include "tz_table.h"
#include <string.h>
#include <stdint.h>

#define OEM_SEND_INTERVAL_US  1000000u  /* 1000 ms */

static uint32_t s_last_send_us;

/* Day of week from date: 1 = Monday .. 7 = Sunday (per PQ46 mEinheiten). Zeller: 0=Sat, 1=Sun, 2=Mon, ... */
static int day_of_week(int year, int month, int day) {
    if (month < 3) { month += 12; year--; }
    int c = year / 100, y = year % 100;
    int w = (c / 4 - 2 * c + y + y / 4 + 13 * (month + 1) / 5 + day - 1) % 7;
    if (w < 0) w += 7;
    return (w == 0) ? 6 : (w == 1) ? 7 : (w - 1);  /* 1=Mon .. 7=Sun */
}

/* Build 8-byte mDiagnose_1 payload. Byte layout per PQ35/46 CAN matrix:
 * Year 7 bits (0-127 = 2000-2127) byte3[4:7], byte4[0:2]
 * Month 4 bits byte4[3:6], Day 5 bits byte4[7], byte5[0:3]
 * Hour 5 bits byte5[4:7], byte6[0], Minute 6 bits byte6[1:6], Second 6 bits byte6[7], byte7[0:4]
 * Zeit_alt 1 bit byte7[7]. */
static void build_mdiagnose_1(uint8_t *data, int year, int month, int day,
                              int hour, int min, int sec, bool zeit_alt) {
    memset(data, 0, 8u);
    int y_val = year - 2000;
    if (y_val < 0) y_val = 0;
    if (y_val > 127) y_val = 127;
    if (month < 1) month = 1;
    if (month > 12) month = 12;
    if (day < 0) day = 0;
    if (day > 31) day = 31;
    if (hour < 0) hour = 0;
    if (hour > 23) hour = 23;
    if (min < 0) min = 0;
    if (min > 59) min = 59;
    if (sec < 0) sec = 0;
    if (sec > 59) sec = 59;

    data[3] = (uint8_t)((y_val & 0x0F) << 4);
    data[4] = (uint8_t)((y_val >> 4) & 0x07);
    data[4] |= (uint8_t)((month & 0x0F) << 3);
    data[4] |= (uint8_t)((day & 1) << 7);
    data[5] = (uint8_t)((day >> 1) & 0x0F);
    data[5] |= (uint8_t)((hour & 0x1F) << 4);
    data[6] = (uint8_t)((hour >> 5) & 1);
    data[6] |= (uint8_t)((min & 0x3F) << 1);
    data[6] |= (uint8_t)((sec & 1) << 7);
    data[7] = (uint8_t)((sec >> 1) & 0x1F);
    if (zeit_alt)
        data[7] |= 0x80u;
}

/* mEinheiten: byte0 bit7=24h(0)/12h(1), bit6=EU(0)/US(1) date, bit1=°C(0)/°F(1), bit0=km(0)/miles(1);
 * byte1 bits 4-6 = Wochentag (0=init, 1=Mon..7=Sun). Default 24h, EU, °C, km, Monday. */
static void build_meinheiten(uint8_t *data, bool use_24h, bool eu_date,
                             int wochentag, bool celsius, bool km) {
    data[0] = 0u;
    if (!use_24h) data[0] |= 0x80u;
    if (!eu_date) data[0] |= 0x40u;
    if (!celsius) data[0] |= 0x02u;
    if (!km) data[0] |= 0x01u;
    data[1] = (uint8_t)((wochentag & 0x07) << 4);
}

void fis_can_oem_tick(const nav_state_t *state, const fis_config_t *config) {
    if (!config || !config->can_enabled)
        return;
    if (!state)
        return;

    uint32_t now = time_us_32();
    if ((now - s_last_send_us) < OEM_SEND_INTERVAL_US)
        return;

    s_last_send_us = now;

    int wochentag = 1;  /* default Monday if no GPS date */

    /* mDiagnose_1: send local time when we have GPS time and position. */
    if (state->position_time_iso8601[0] != '\0' &&
        state->lat_centideg != NAV_POS_INVALID &&
        state->lon_centideg != NAV_POS_INVALID) {
        int y, mo, d, h, mi, s;
        if (local_time_get_local_components(state->position_time_iso8601,
                                            (tz_coord_t)state->lat_centideg,
                                            (tz_coord_t)state->lon_centideg,
                                            &y, &mo, &d, &h, &mi, &s)) {
            uint8_t payload[8];
            build_mdiagnose_1(payload, y, mo, d, h, mi, s, false);
            fis_can_frame_t frame = {
                .id = FIS_CAN_OEM_ID_MDIAGNOSE_1,
                .rtr = false,
                .dlc = 8,
            };
            memcpy(frame.data, payload, 8u);
            fis_can_send(&frame);
            wochentag = day_of_week(y, mo, d);
        }
    }

    /* mEinheiten: 24h, EU date, day of week from GPS (or Monday), °C, km. */
    {
        uint8_t payload[2];
        build_meinheiten(payload, true, true, wochentag, true, true);
        fis_can_frame_t frame = {
            .id = FIS_CAN_OEM_ID_MEINHEITEN,
            .rtr = false,
            .dlc = 2,
        };
        frame.data[0] = payload[0];
        frame.data[1] = payload[1];
        fis_can_send(&frame);
    }
}
