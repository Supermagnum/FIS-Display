/*
 * FIS display test firmware.
 * Cycles through all display modes (nav, call, media, clock) every 4 seconds
 * so the user can verify the display and all text/icons work.
 *
 * No serial input; demo state is generated internally.
 */

#include <string.h>

#include "pico/stdlib.h"
#include "pico/multicore.h"
#include "pico/time.h"

#include "nav_state.h"
#include "fis_config.h"
#include "fis_rx.h"
#include "fis_display.h"
#include "fis_nav_icons.h"

#define DEMO_CYCLE_MS  4000
/* Nav text (5) + icon cycle (FIS_ICON_COUNT) + call (1) + media (1) + clock (1) */
#define NUM_PHASES     (5 + FIS_ICON_COUNT + 3)

static void set_demo_nav(nav_state_t *s, uint32_t distance_m,
                         const char *maneuver, const char *street) {
    memset(s, 0, sizeof(*s));
    s->status       = NAV_STATUS_ROUTING;
    s->distance_m   = distance_m;
    strncpy(s->maneuver, maneuver, sizeof(s->maneuver) - 1);
    s->maneuver[sizeof(s->maneuver) - 1] = '\0';
    strncpy(s->street, street, sizeof(s->street) - 1);
    s->street[sizeof(s->street) - 1] = '\0';
}

static void set_demo_call(nav_state_t *s, const char *caller) {
    memset(s, 0, sizeof(*s));
    s->call_active = true;
    strncpy(s->caller, caller, sizeof(s->caller) - 1);
    s->caller[sizeof(s->caller) - 1] = '\0';
}

static void set_demo_media(nav_state_t *s, const char *track) {
    memset(s, 0, sizeof(*s));
    strncpy(s->media, track, sizeof(s->media) - 1);
    s->media[sizeof(s->media) - 1] = '\0';
}

/* Clock demo: fixed UTC time and Berlin coords so local time (CET/CEST) works. */
static void set_demo_clock(nav_state_t *s, fis_config_t *cfg) {
    memset(s, 0, sizeof(*s));
    strncpy(s->position_time_iso8601, "2025-03-15T14:30:00Z",
            sizeof(s->position_time_iso8601) - 1);
    s->position_time_iso8601[sizeof(s->position_time_iso8601) - 1] = '\0';
    s->lat_centideg = 5252;   /* 52.52 N Berlin */
    s->lon_centideg = 1340;   /* 13.40 E Berlin */
    s->remain_m     = 5000;
    s->eta_unix     = 1742040600u;  /* example */
    s->heading_deg  = 90;     /* E */
    fis_config_set_defaults(cfg);
}

static void core1_demo_loop(void) {
    nav_state_t state;
    fis_config_t config;
    uint8_t phase = 0;
    absolute_time_t next_switch = make_timeout_time_ms(DEMO_CYCLE_MS);

    for (;;) {
        fis_rx_task();

        if (get_absolute_time() >= next_switch) {
            phase = (phase + 1) % NUM_PHASES;
            next_switch = make_timeout_time_ms(DEMO_CYCLE_MS);
        }

        if (!fis_bus_idle()) {
            tight_loop_contents();
            continue;
        }

        switch (phase) {
        case 0:
            set_demo_nav(&state, 1200, "LEFT", "Main St");
            fis_display_inject_nav(&state);
            break;
        case 1:
            set_demo_nav(&state, 500, "RIGHT", "Highway A1");
            fis_display_inject_nav(&state);
            break;
        case 2:
            set_demo_nav(&state, 2000, "STRAIGHT", "Boulevard");
            fis_display_inject_nav(&state);
            break;
        case 3:
            set_demo_nav(&state, 300, "EXIT L", "Exit 42");
            fis_display_inject_nav(&state);
            break;
        case 4:
            set_demo_nav(&state, 1000, "RNDABOUT", "Ring Rd");
            fis_display_inject_nav(&state);
            break;
        default: {
            const unsigned p = (unsigned)phase;
            if (p < 5u + FIS_ICON_COUNT) {
                fis_display_inject_icon((uint8_t)(p - 5u));
            } else if (p == 5u + FIS_ICON_COUNT) {
                set_demo_call(&state, "Test Caller");
                fis_display_inject_call(&state);
            } else if (p == 6u + FIS_ICON_COUNT) {
                set_demo_media(&state, "Test Track");
                fis_display_inject_media(&state);
            } else {
                set_demo_clock(&state, &config);
                fis_display_inject_clock(&state, &config);
            }
            break;
        }
        }

        sleep_ms(20);
    }
}

int main(void) {
    stdio_init_all();

    fis_config_t config;
    fis_config_set_defaults(&config);

    fis_rx_init(pio0, 0, FIS_PIN_CLK, FIS_PIN_DATA);
    fis_display_init(pio0, 1, FIS_PIN_DATA_OUT, FIS_PIN_CLK_OUT);

    multicore_launch_core1(core1_demo_loop);

    for (;;) {
        sleep_ms(1000);
    }
    return 0;
}
