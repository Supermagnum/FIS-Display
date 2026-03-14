 #ifndef FIS_DISPLAY_H
 #define FIS_DISPLAY_H

 #include <stdbool.h>
 #include <stddef.h>
 #include <stdint.h>

 #include "pico/stdlib.h"
 #include "hardware/pio.h"

 #include "nav_state.h"
 #include "fis_config.h"

 // Fixed pin assignment for the TX side.
 #define FIS_PIN_ENA_OUT  3u
 #define FIS_PIN_CLK_OUT  4u
 #define FIS_PIN_DATA_OUT 5u

 void fis_display_init(PIO pio, uint sm, uint data_pin, uint clk_pin);

 // Returns true while the TX state machine is still busy transmitting.
 bool fis_display_busy(void);

 // High-level helpers to inject content based on navigation/media state.
 void fis_display_inject_nav(const nav_state_t *state);
 void fis_display_inject_media(const nav_state_t *state);
 void fis_display_inject_call(const nav_state_t *state);
 void fis_display_inject_clock(const nav_state_t *state, const fis_config_t *config);

 #endif

