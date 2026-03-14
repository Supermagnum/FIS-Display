#include "serial_parser.h"

#include <string.h>
#include <stdlib.h>

#include "pico/stdio.h"

#define LINE_BUF_LEN 128

static void handle_line(const char *line, nav_state_t *state, fis_config_t *config,
                        critical_section_t *lock) {
    if (!line || !state || !lock) {
        return;
    }

    critical_section_enter_blocking(lock);

    if (strncmp(line, "NAV:TURN:", 9) == 0) {
        strncpy(state->maneuver, line + 9, sizeof state->maneuver);
        state->maneuver[sizeof state->maneuver - 1] = '\0';
    } else if (strncmp(line, "NAV:DIST:", 9) == 0) {
        state->distance_m = (uint32_t)strtoul(line + 9, NULL, 10);
    } else if (strncmp(line, "NAV:STREET:", 11) == 0) {
        strncpy(state->street, line + 11, sizeof state->street);
        state->street[sizeof state->street - 1] = '\0';
    } else if (strncmp(line, "NAV:STATUS:", 11) == 0) {
        const char *s = line + 11;
        if (strcmp(s, "routing") == 0) {
            state->status = NAV_STATUS_ROUTING;
        } else if (strcmp(s, "recalculating") == 0) {
            state->status = NAV_STATUS_RECALCULATING;
        } else {
            state->status = NAV_STATUS_NO_ROUTE;
        }
    } else if (strncmp(line, "NAV:ETA:", 8) == 0) {
        state->eta_unix = (uint32_t)strtoul(line + 8, NULL, 10);
    } else if (strncmp(line, "NAV:REMAIN:", 11) == 0) {
        state->remain_m = (uint32_t)strtoul(line + 11, NULL, 10);
    } else if (strncmp(line, "NAV:HEAD:", 9) == 0) {
        unsigned long v = strtoul(line + 9, NULL, 10);
        state->heading_deg = (v <= 360u) ? (uint16_t)v : NAV_HEADING_INVALID;
    } else if (strncmp(line, "BT:TRACK:", 9) == 0) {
        strncpy(state->media, line + 9, sizeof state->media);
        state->media[sizeof state->media - 1] = '\0';
    } else if (strncmp(line, "BT:CALL:", 8) == 0) {
        strncpy(state->caller, line + 8, sizeof state->caller);
        state->caller[sizeof state->caller - 1] = '\0';
        state->call_active = true;
    } else if (strncmp(line, "BT:CALLEND", 10) == 0) {
        state->call_active = false;
        state->caller[0] = '\0';
    } else if (strncmp(line, "NAV:TIME:", 9) == 0) {
        size_t len = strlen(line + 9);
        if (len >= sizeof state->position_time_iso8601) {
            len = sizeof state->position_time_iso8601 - 1;
        }
        memcpy(state->position_time_iso8601, line + 9, len);
        state->position_time_iso8601[len] = '\0';
    } else if (strncmp(line, "NAV:POS:", 8) == 0) {
        char *end;
        double lat = strtod(line + 8, &end);
        if (*end == ',' || *end == ';') {
            double lon = strtod(end + 1, &end);
            state->lat_centideg = (int16_t)(lat * 100.0);
            state->lon_centideg = (int16_t)(lon * 100.0);
        }
    } else if (config && strncmp(line, "CFG:", 4) == 0) {
        const char *v = strchr(line + 4, ':');
        unsigned val = (v && v[1] == '1') ? 1u : 0u;
        if (strncmp(line + 4, "CLOCK:", 6) == 0) {
            config->show_clock = (val != 0);
        } else if (strncmp(line + 4, "ETA:", 4) == 0) {
            config->show_eta = (val != 0);
        } else if (strncmp(line + 4, "COMPASS:", 8) == 0) {
            config->show_compass = (val != 0);
        } else if (strncmp(line + 4, "REMAIN:", 7) == 0) {
            config->show_remain = (val != 0);
        } else if (strncmp(line + 4, "CAN:", 4) == 0) {
            config->can_enabled = (val != 0);
        }
    }

    critical_section_exit(lock);
}

static void feed_char(char c, char *buf, size_t *idx, nav_state_t *state, fis_config_t *config,
                     critical_section_t *lock) {
    if (c == '\r') {
        return;
    }
    if (c == '\n') {
        buf[*idx] = '\0';
        if (*idx > 0) {
            handle_line(buf, state, config, lock);
        }
        *idx = 0;
        return;
    }

    if (*idx + 1 < LINE_BUF_LEN) {
        buf[*idx] = c;
        (*idx)++;
    }
}

void serial_parser_init(void) {
    stdio_init_all();
}

void serial_parser_poll(nav_state_t *state, fis_config_t *config, critical_section_t *lock) {
    static char usb_buf[LINE_BUF_LEN];
    static size_t usb_idx = 0;

    int ch;
    while ((ch = getchar_timeout_us(0)) != PICO_ERROR_TIMEOUT) {
        feed_char((char)ch, usb_buf, &usb_idx, state, config, lock);
    }
}
