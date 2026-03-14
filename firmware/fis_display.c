 #include "fis_display.h"

 #include <ctype.h>
 #include <stdio.h>
 #include <string.h>

 #include "hardware/gpio.h"

 #include "fis_3lb_tx.pio.h"
 #include "fis_nav_icons.h"
 #include "local_time.h"
 #include "tz_lookup.h"

 /* TLBFISLib-style 3LB graphic opcodes and block size */
 #define FIS_3LB_CLEAR_BYTE   0x53u
 #define FIS_3LB_BITMAP_BYTE  0x55u
 #define FIS_3LB_CLAIM_CLEAR  0x82u  /* claim and clear, normal color */
 #define FIS_3LB_BMP_OR       0x02u  /* bitmap OR output (normal) */
 #define FIS_3LB_BLOCK_MAX    42u    /* max bytes per 3LB frame */

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

 /* Send claim/clear screen (0x53): fullscreen 64x88, normal color. */
 static void send_claim_screen(void) {
     const uint8_t frame[] = {
         FIS_3LB_CLEAR_BYTE, 5, FIS_3LB_CLAIM_CLEAR,
         0, 0, 64, 88
     };
     fis_send_bytes(frame, sizeof frame);
 }

 /* Send one bitmap block (0x55): option, x, y, then pixel rows. */
 static void send_bitmap_block(uint8_t x, uint8_t y, const uint8_t *pixels,
                               size_t pixel_len) {
     if (pixel_len > FIS_3LB_BLOCK_MAX - 5u) {
         pixel_len = FIS_3LB_BLOCK_MAX - 5u;
     }
     uint8_t frame[FIS_3LB_BLOCK_MAX];
     size_t len = 0;
     frame[len++] = FIS_3LB_BITMAP_BYTE;
     frame[len++] = (uint8_t)(pixel_len + 3u);
     frame[len++] = FIS_3LB_BMP_OR;
     frame[len++] = x;
     frame[len++] = y;
     memcpy(&frame[len], pixels, pixel_len);
     len += pixel_len;
     fis_send_bytes(frame, len);
 }

 /* Inject a 1-bit bitmap at (x,y), width w, height h. Data is row-major, (w+7)/8 bytes per row. */
 void fis_display_inject_bitmap(uint8_t x, uint8_t y, uint8_t w, uint8_t h,
                                const uint8_t *bitmap) {
     if (!bitmap || w == 0 || h == 0) {
         return;
     }
     send_claim_screen();

     const unsigned bytes_per_line = (unsigned)(w + 7u) / 8u;
     const unsigned payload_max   = (unsigned)(FIS_3LB_BLOCK_MAX - 5u);
     const unsigned lines_per_block = payload_max / bytes_per_line;
     if (lines_per_block == 0u) {
         return;
     }

     unsigned row = 0;
     while (row < (unsigned)h) {
         unsigned lines_this = (unsigned)h - row;
         if (lines_this > lines_per_block) {
             lines_this = lines_per_block;
         }
         size_t pixel_bytes = lines_this * bytes_per_line;
         send_bitmap_block((uint8_t)x, (uint8_t)(y + row), bitmap + row * bytes_per_line, pixel_bytes);
         row += lines_this;
     }
 }

 void fis_display_inject_icon(uint8_t icon_index) {
     if (icon_index >= FIS_ICON_COUNT) {
         return;
     }
     const FISIcon *icon = &FIS_ICON_TABLE[icon_index];
     if (!icon->data || icon->w == 0 || icon->h == 0) {
         return;
     }
     fis_display_inject_bitmap(0, 0, icon->w, icon->h, icon->data);
 }

 /* Map maneuver string (from NAV:TURN:<code>) to FIS_ICON_TABLE index. Returns -1 if no match. */
 int fis_maneuver_to_icon_index(const char *maneuver) {
     if (!maneuver || !maneuver[0]) {
         return -1;
     }
     char buf[32];
     size_t i = 0;
     for (const char *s = maneuver; *s && i < sizeof(buf) - 1; s++) {
         char c = (char)(unsigned char)*s;
         if (c == ' ' || c == '-') {
             buf[i++] = '_';
         } else {
             buf[i++] = (char)(unsigned char)tolower((unsigned char)c);
         }
     }
     buf[i] = '\0';

     /* Match normalized string to icon index. Order: specific first, then generic. */
     if (strcmp(buf, "destination") == 0) return 0;
     if (strcmp(buf, "exit_left") == 0 || strcmp(buf, "exit_l") == 0) return 1;
     if (strcmp(buf, "exit_right") == 0 || strcmp(buf, "exit_r") == 0) return 2;
     if (strcmp(buf, "keep_left") == 0 || strcmp(buf, "keepleft") == 0) return 3;
     if (strcmp(buf, "keep_right") == 0 || strcmp(buf, "keepright") == 0) return 4;
     if (strcmp(buf, "left_3") == 0 || strcmp(buf, "left3") == 0) return 7;
     if (strcmp(buf, "left_2") == 0 || strcmp(buf, "left2") == 0) return 6;
     if (strcmp(buf, "left_1") == 0 || strcmp(buf, "left1") == 0 || strcmp(buf, "left") == 0) return 5;
     if (strcmp(buf, "merge_left") == 0 || strcmp(buf, "mergeleft") == 0) return 8;
     if (strcmp(buf, "merge_right") == 0 || strcmp(buf, "mergeright") == 0) return 9;
     if (strcmp(buf, "right_3") == 0 || strcmp(buf, "right3") == 0) return 12;
     if (strcmp(buf, "right_2") == 0 || strcmp(buf, "right2") == 0) return 11;
     if (strcmp(buf, "right_1") == 0 || strcmp(buf, "right1") == 0 || strcmp(buf, "right") == 0) return 10;
     if (strncmp(buf, "roundabout_r", 12) == 0 && buf[12] >= '1' && buf[12] <= '8' && buf[13] == '\0') {
         int exit_no = buf[12] - '1';
         return 21 + (exit_no >= 0 ? exit_no : 0);
     }
     if (strncmp(buf, "roundabout_l", 12) == 0 && buf[12] >= '1' && buf[12] <= '8' && buf[13] == '\0') {
         int exit_no = buf[12] - '1';
         return 13 + (exit_no >= 0 ? exit_no : 0);
     }
     if (strcmp(buf, "roundabout") == 0 || strcmp(buf, "rndabout") == 0 ||
         strcmp(buf, "roundabout_l1") == 0) return 13;
     if (strcmp(buf, "straight") == 0) return 29;
     if (strcmp(buf, "turnaround_left") == 0 || strcmp(buf, "turn_around_left") == 0) return 30;
     if (strcmp(buf, "turnaround_right") == 0 || strcmp(buf, "turn_around_right") == 0) return 31;
     return -1;
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

    if (line0) {
        size_t l = strlen(line0);
        if (l > 8) l = 8;
        if (l > 0) memcpy(&buf[3], line0, l);
    }
    if (line1) {
        size_t l = strlen(line1);
        if (l > 8) l = 8;
        if (l > 0) memcpy(&buf[11], line1, l);
    }

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

 /* Heading in degrees (0-360) to compass string (N, NE, E, SE, S, SW, W, NW). Max 2 chars + null. */
 static void heading_to_compass(uint16_t deg, char *out, size_t out_len) {
     static const char *const dirs[] = { "N", "NE", "E", "SE", "S", "SW", "W", "NW" };
     if (out_len < 3) return;
     if (deg == NAV_HEADING_INVALID) {
         out[0] = '\0';
         return;
     }
     unsigned i = ((unsigned)deg + 22u) / 45u;
     if (i > 7u) i = 0u;
     (void)snprintf(out, out_len, "%s", dirs[i]);
 }

 void fis_display_inject_clock(const nav_state_t *state, const fis_config_t *config) {
     char line0[16];
     char line1[16];

     line0[0] = '\0';
     line1[0] = '\0';

     if (!config || !config->show_clock) {
         return;
     }
     if (state->position_time_iso8601[0] == '\0') {
         return;
     }
     if (!local_time_format_fis(state->position_time_iso8601,
                               (tz_coord_t)state->lat_centideg,
                               (tz_coord_t)state->lon_centideg,
                               line0, sizeof line0)) {
         return;
     }
     /* Line 2: remaining distance, ETA (local time), compass, or date (by toggle and priority) */
     if (config->show_remain && state->remain_m != 0u) {
         if (state->remain_m >= 1000u) {
             float km = state->remain_m / 1000.0f;
             (void)snprintf(line1, sizeof line1, "%.1fkm", km);
         } else {
             (void)snprintf(line1, sizeof line1, "%um", (unsigned)state->remain_m);
         }
     }
     if (line1[0] == '\0' && config->show_eta && state->eta_unix != 0u) {
         if (local_time_format_eta(state->eta_unix,
                                   (tz_coord_t)state->lat_centideg,
                                   (tz_coord_t)state->lon_centideg,
                                   line1, sizeof line1)) {
             /* line1 now e.g. "ARR14:32" */
         }
     }
     if (line1[0] == '\0' && config->show_compass && state->heading_deg != NAV_HEADING_INVALID) {
         heading_to_compass(state->heading_deg, line1, sizeof line1);
     }
     if (line1[0] == '\0') {
         int y, mo, d, h, mi, s;
         if (local_time_parse_iso8601_utc(state->position_time_iso8601, &y, &mo, &d, &h, &mi, &s)) {
             int ri = tz_lookup_rule_idx((tz_coord_t)state->lat_centideg, (tz_coord_t)state->lon_centideg);
             if (ri >= 0) {
                 const tz_rule_t *rule = tz_get_rule((uint8_t)ri);
                 if (rule) {
                     int ly, lm, ld, lh, lmin, ls;
                     tz_utc_to_local(rule, y, mo, d, h, mi, s, &ly, &lm, &ld, &lh, &lmin, &ls);
                     local_time_format_date_short(ly, lm, ld, line1, sizeof line1);
                 }
             }
         }
     }
     if (line1[0] == '\0') {
         (void)snprintf(line1, sizeof line1, "        ");
     }

     uint8_t frame[64];
     size_t  len = 0;
     build_text_frame(line0, line1, frame, &len, sizeof frame);
     fis_send_bytes(frame, len);
 }

