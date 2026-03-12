 #ifndef FIS_RX_H
 #define FIS_RX_H

 #include <stdbool.h>
 #include "pico/stdlib.h"
 #include "hardware/pio.h"

 // Fixed pin assignment for the 3LB bus on the Pico side.
 // All pins must be connected via 5V↔3.3V level shifters.
 #define FIS_PIN_ENA  0u
 #define FIS_PIN_CLK  1u
 #define FIS_PIN_DATA 2u

 void fis_rx_init(PIO pio, uint sm, uint clk_pin, uint data_pin);

 // Call frequently from the 3LB handling core to update idle/activity state.
 void fis_rx_task(void);

 // Returns true if the bus has been idle (ENA high) for at least the
 // configured idle threshold (a few milliseconds).
 bool fis_bus_idle(void);

 #endif

