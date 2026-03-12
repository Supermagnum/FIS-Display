 #ifndef SERIAL_PARSER_H
 #define SERIAL_PARSER_H

 #include "pico/stdlib.h"
 #include "hardware/sync.h"

 #include "nav_state.h"

 void serial_parser_init(void);

 // Poll USB CDC and Bluetooth SPP for newline-terminated NAV:* and BT:*
// lines; parse and update nav_state. Same protocol on both transports.
 void serial_parser_poll(nav_state_t *state, critical_section_t *lock);

 #endif

