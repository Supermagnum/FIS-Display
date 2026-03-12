 #include <string.h>

 #include "pico/stdlib.h"
 #include "pico/multicore.h"
 #include "hardware/sync.h"
 #include "hardware/pio.h"

 #include "nav_state.h"
 #include "fis_rx.h"
 #include "fis_display.h"
 #include "serial_parser.h"

 static nav_state_t        g_nav_state;
 static critical_section_t g_nav_lock;

 static void nav_state_init_struct(nav_state_t *s) {
     memset(s, 0, sizeof *s);
     s->status = NAV_STATUS_NO_ROUTE;
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

 static void core1_3lb_loop(void) {
     nav_state_t local_state;

     for (;;) {
         // Update RX monitor (ENA-based idle detection).
         fis_rx_task();

         if (!fis_bus_idle()) {
             tight_loop_contents();
             continue;
         }

         // Snapshot navigation state under lock.
         critical_section_enter_blocking(&g_nav_lock);
         nav_state_copy(&local_state, &g_nav_state);
         critical_section_exit(&g_nav_lock);

         // Decide what, if anything, to inject.
         if (local_state.call_active) {
             fis_display_inject_call(&local_state);
         } else if (should_inject_nav(&local_state)) {
             fis_display_inject_nav(&local_state);
         } else if (has_media(&local_state)) {
             fis_display_inject_media(&local_state);
         }

         // Brief pause to avoid saturating the bus with back-to-back frames.
         sleep_ms(20);
     }
 }

 int main(void) {
     nav_state_init_struct(&g_nav_state);
     critical_section_init(&g_nav_lock);

     // Initialise serial I/O (USB + Bluetooth HC-05).
     serial_parser_init();

     // Initialise 3LB RX and TX on PIO0.
     fis_rx_init(pio0, 0, FIS_PIN_CLK, FIS_PIN_DATA);
     fis_display_init(pio0, 1, FIS_PIN_DATA_OUT, FIS_PIN_CLK_OUT);

     // Launch core 1 to run the 3LB loop.
     multicore_launch_core1(core1_3lb_loop);

     // Core 0: handle serial input and update nav_state.
     for (;;) {
         serial_parser_poll(&g_nav_state, &g_nav_lock);
         sleep_ms(5);
     }
 }

