 #include "fis_display.h"

 #include <string.h>

 #include "hardware/gpio.h"

 #include "fis_3lb_tx.pio.h"

 static PIO  s_pio      = pio0;
 static uint s_sm       = 1;
 static uint s_data_pin = FIS_PIN_DATA_OUT;
 static uint s_clk_pin  = FIS_PIN_CLK_OUT;

// Simple helper to send a raw TLBFISLib-style payload as one 3LB frame.
// Bytes are inverted on the wire, matching TLBLib::send_byte().
 static void fis_send_bytes(const uint8_t *data, size_t len) {
     if (!len) {
         return;
     }

     // Assert ENA low for the duration of this frame.
     gpio_put(FIS_PIN_ENA_OUT, 0);

    for (size_t i = 0; i < len; i++) {
        // Wait for space in the FIFO before queuing the next byte.
        while (pio_sm_is_tx_fifo_full(s_pio, s_sm)) {
            tight_loop_contents();
        }
        // TLBLib transfers inverted bytes (0xFF ^ data).
        pio_sm_put(s_pio, s_sm, (uint32_t)(0xFFu ^ data[i]));
    }

     // Wait for transmission to complete.
     while (!fis_display_busy()) {
         tight_loop_contents();
     }

     // Release ENA high, bus back to idle.
     gpio_put(FIS_PIN_ENA_OUT, 1);
 }

 void fis_display_init(PIO pio, uint sm, uint data_pin, uint clk_pin) {
     s_pio      = pio;
     s_sm       = sm;
     s_data_pin = data_pin;
     s_clk_pin  = clk_pin;

     // Configure ENA as output, idle high.
     gpio_init(FIS_PIN_ENA_OUT);
     gpio_set_dir(FIS_PIN_ENA_OUT, GPIO_OUT);
     gpio_put(FIS_PIN_ENA_OUT, 1);

     // Set up CLK pin for side-set and DATA as out_base.
     pio_gpio_init(s_pio, s_data_pin);
     pio_gpio_init(s_pio, s_clk_pin);

     uint offset = pio_add_program(s_pio, &fis_3lb_tx_program);
     pio_sm_config c = fis_3lb_tx_program_get_default_config(offset);

     sm_config_set_out_pins(&c, s_data_pin, 1);
     sm_config_set_sideset_pins(&c, s_clk_pin);

     // Shift right, autopull after 8 bits.
     sm_config_set_out_shift(&c, true, true, 8);

     pio_sm_init(s_pio, s_sm, offset, &c);
     pio_sm_set_enabled(s_pio, s_sm, true);
 }

 bool fis_display_busy(void) {
     // Busy while there is data in the TX FIFO or shift register.
     if (!pio_sm_is_tx_fifo_empty(s_pio, s_sm)) {
         return false;
     }
     return (s_pio->fdebug & (1u << (PIO_FDEBUG_TXOV_BITS + s_sm))) != 0u;
 }

static void build_text_frame(const char *line0, const char *line1,
                             uint8_t *buf, size_t *len, size_t max_len) {
    // Match TLBFISLib radio text format (see TLBFISLib::_writeRadioText):
    //
    //  [0] = 0x81       (radio_byte opcode)
    //  [1] = 0x11       (length byte used by TLBLib)
    //  [2] = 0xF0       (options)
    //  [3..10]  = line 0 text (max 8 chars, padded with 0)
    //  [11..18] = line 1 text (max 8 chars, padded with 0)
    //
    // Total payload length = 19 bytes.

    const size_t needed = 19;
    if (max_len < needed) {
        *len = 0;
        return;
    }

    memset(buf, 0x00, needed);

    buf[0] = 0x81u;
    buf[1] = 0x11u;
    buf[2] = 0xF0u;

    // Helper to copy up to 8 characters into the given slot.
    auto copy_line = [](const char *src, uint8_t *dst) {
        if (!src) {
            return;
        }
        size_t l = strlen(src);
        if (l > 8) {
            l = 8;
        }
        if (l > 0) {
            memcpy(dst, src, l);
        }
    };

    copy_line(line0, &buf[3]);   // line 0
    copy_line(line1, &buf[11]);  // line 1

    *len = needed;
 }

 void fis_display_inject_nav(const nav_state_t *state) {
     char line0[32];
     char line1[32];

     // Format distance and maneuver.
     if (state->distance_m >= 1000) {
         float km = state->distance_m / 1000.0f;
         snprintf(line0, sizeof line0, "%.1fkm %s", km, state->maneuver);
     } else {
         snprintf(line0, sizeof line0, "%um %s", (unsigned)state->distance_m, state->maneuver);
     }

     strncpy(line1, state->street, sizeof line1);
     line1[sizeof line1 - 1] = '\0';

     uint8_t frame[64];
     size_t  len = 0;
     build_text_frame(line0, line1, frame, &len, sizeof frame);
     fis_send_bytes(frame, len);
 }

 void fis_display_inject_media(const nav_state_t *state) {
     char line0[32];
     char line1[32];

     strncpy(line0, "TRACK", sizeof line0);
     line0[sizeof line0 - 1] = '\0';

     strncpy(line1, state->media, sizeof line1);
     line1[sizeof line1 - 1] = '\0';

     uint8_t frame[64];
     size_t  len = 0;
     build_text_frame(line0, line1, frame, &len, sizeof frame);
     fis_send_bytes(frame, len);
 }

 void fis_display_inject_call(const nav_state_t *state) {
     char line0[32];
     char line1[32];

     strncpy(line0, "CALL", sizeof line0);
     line0[sizeof line0 - 1] = '\0';

     strncpy(line1, state->caller, sizeof line1);
     line1[sizeof line1 - 1] = '\0';

     uint8_t frame[64];
     size_t  len = 0;
     build_text_frame(line0, line1, frame, &len, sizeof frame);
     fis_send_bytes(frame, len);
 }

