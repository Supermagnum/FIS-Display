 #include <string.h>

 #include "pico/stdlib.h"
 #include "pico/multicore.h"
 #include "hardware/sync.h"
 #include "hardware/pio.h"

 #include "nav_state.h"
 #include "fis_config.h"
 #include "fis_rx.h"
 #include "fis_display.h"
 #include "serial_parser.h"

 static nav_state_t        g_nav_state;
 static fis_config_t       g_fis_config;
 static critical_section_t g_nav_lock;

 static void nav_state_init_struct(nav_state_t *s) {
     memset(s, 0, sizeof *s);
     s->status = NAV_STATUS_NO_ROUTE;
     s->lat_centideg = NAV_POS_INVALID;
     s->lon_centideg = NAV_POS_INVALID;
     s->heading_deg = NAV_HEADING_INVALID;
 }

 static void nav_state_copy(nav_state_t *dst, const nav_state_t *src) {
     memcpy(dst, src, sizeof *dst);
 }

 static bool should_inject_nav(const nav_state_t *s) {
     return s->status == NAV_STATUS_ROUTING ||
            s->status == NAV_STATUS_RECALCULATING;
 }

 static bool has_media(const nav_state_t *s) {
     return s->media[0] != '\0';
 }

 static bool has_clock(const nav_state_t *s, const fis_config_t *cfg) {
     return cfg->show_clock && s->position_time_iso8601[0] != '\0';
 }

 static void core1_3lb_loop(void) {
     nav_state_t local_state;

     for (;;) {
         // Update RX monitor (ENA-based idle detection).
         fis_rx_task();

         if (!fis_bus_idle()) {
             tight_loop_contents();
             continue;
         }

         // Snapshot navigation state and config under lock.
         critical_section_enter_blocking(&g_nav_lock);
         nav_state_copy(&local_state, &g_nav_state);
         fis_config_t local_config = g_fis_config;
         critical_section_exit(&g_nav_lock);

         // Decide what, if anything, to inject.
         if (local_state.call_active) {
             fis_display_inject_call(&local_state);
         } else if (should_inject_nav(&local_state)) {
             fis_display_inject_nav(&local_state);
         } else if (has_media(&local_state)) {
             fis_display_inject_media(&local_state);
         } else if (has_clock(&local_state, &local_config)) {
             fis_display_inject_clock(&local_state, &local_config);
         }

         // Brief pause to avoid saturating the bus with back-to-back frames.
         sleep_ms(20);
     }
 }

 int main(void) {
     nav_state_init_struct(&g_nav_state);
     fis_config_set_defaults(&g_fis_config);
     critical_section_init(&g_nav_lock);

     // Initialise serial I/O (USB CDC and Bluetooth SPP via BTstack).
     serial_parser_init();

     // Initialise 3LB RX and TX on PIO0.
     fis_rx_init(pio0, 0, FIS_PIN_CLK, FIS_PIN_DATA);
     fis_display_init(pio0, 1, FIS_PIN_DATA_OUT, FIS_PIN_CLK_OUT);

     // Launch core 1 to run the 3LB loop.
     multicore_launch_core1(core1_3lb_loop);

     // Core 0: handle serial input and update nav_state and config.
     for (;;) {
         serial_parser_poll(&g_nav_state, &g_fis_config, &g_nav_lock);
         sleep_ms(5);
     }
 }

