 #ifndef SERIAL_PARSER_H
 #define SERIAL_PARSER_H

 #include "pico/stdlib.h"
 #include "hardware/sync.h"

 #include "nav_state.h"
 #include "fis_config.h"

 void serial_parser_init(void);

 /* Poll USB CDC and Bluetooth SPP for newline-terminated lines; update state and config.
  * Protocol: NAV:*, BT:*, CFG:CLOCK:0|1, CFG:ETA:0|1, CFG:COMPASS:0|1, CFG:REMAIN:0|1. */
 void serial_parser_poll(nav_state_t *state, fis_config_t *config, critical_section_t *lock);

 #endif

