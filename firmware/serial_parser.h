 #ifndef SERIAL_PARSER_H
 #define SERIAL_PARSER_H

 #include "pico/stdlib.h"
 #include "hardware/sync.h"

 #include "nav_state.h"

 void serial_parser_init(void);

 // Poll USB CDC and UART1, parse complete lines and update nav_state.
 void serial_parser_poll(nav_state_t *state, critical_section_t *lock);

 #endif

