 #include "fis_rx.h"

 #include "hardware/gpio.h"
 #include "pico/time.h"

 #include "fis_3lb_rx.pio.h"

// PIO/SM used for receive.
static PIO  s_pio      = pio0;
static uint s_sm       = 0;
static uint s_clk_pin  = FIS_PIN_CLK;
static uint s_data_pin = FIS_PIN_DATA;

// Last time ENA was observed low (active frame) and last ENA level.
static absolute_time_t s_last_active_time;
static bool            s_last_ena_level = true;

 // Consider the bus idle if ENA is high and no activity was seen
 // for at least this many microseconds.
 static const uint32_t BUS_IDLE_US = 2000; /* 2 ms */

 void fis_rx_init(PIO pio, uint sm, uint clk_pin, uint data_pin) {
     s_pio      = pio;
     s_sm       = sm;
     s_clk_pin  = clk_pin;
     s_data_pin = data_pin;

     // Configure ENA as input (cluster-side level shifted signal).
     gpio_init(FIS_PIN_ENA);
     gpio_set_dir(FIS_PIN_ENA, GPIO_IN);

     // Load and configure PIO program.
     uint offset = pio_add_program(s_pio, &fis_3lb_rx_program);

     pio_sm_config c = fis_3lb_rx_program_get_default_config(offset);
     // DATA on in_base
     sm_config_set_in_pins(&c, s_data_pin);
     // CLK as jmp pin
     sm_config_set_jmp_pin(&c, s_clk_pin);

     // Shift left, autopush after 8 bits into RX FIFO.
     sm_config_set_in_shift(&c, true, true, 8);

     pio_sm_init(s_pio, s_sm, offset, &c);
     pio_sm_set_enabled(s_pio, s_sm, true);

     s_last_active_time = get_absolute_time();
     s_last_ena_level   = gpio_get(FIS_PIN_ENA);
 }

 void fis_rx_task(void) {
     // Monitor ENA level. Active frames pull ENA low.
     bool ena = gpio_get(FIS_PIN_ENA);
     if (!ena) {
         s_last_active_time = get_absolute_time();
     }

     // If ENA has just transitioned high, a frame ended. Any partial byte
     // currently in the shift register would otherwise be combined with the
     // next frame. Reset the RX state machine and clear its FIFOs so that
     // each frame starts with a clean byte boundary.
     if (ena && !s_last_ena_level) {
         pio_sm_clear_fifos(s_pio, s_sm);
         pio_sm_restart(s_pio, s_sm);
     }
     s_last_ena_level = ena;

     // Drain RX FIFO to avoid overflow, even if we do not yet decode frames.
     while (!pio_sm_is_rx_fifo_empty(s_pio, s_sm)) {
         (void)pio_sm_get(s_pio, s_sm);
     }
 }

 bool fis_bus_idle(void) {
     // Bus is idle when ENA is high and enough time has passed
     // since the last low period.
     if (!gpio_get(FIS_PIN_ENA)) {
         return false;
     }

     int64_t dt = absolute_time_diff_us(s_last_active_time, get_absolute_time());
     return dt >= BUS_IDLE_US;
 }

