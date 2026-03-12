 #include "serial_parser.h"

 #include <string.h>
 #include <stdio.h>

 #include "pico/stdio.h"
 #include "hardware/uart.h"

 // UART instance for Bluetooth HC-05 (GPIO 8/9).
 #define BT_UART       uart1
 #define BT_UART_BAUD  115200
 #define BT_UART_TX_PIN 9u
 #define BT_UART_RX_PIN 8u

 #define LINE_BUF_LEN 128

 static void handle_line(const char *line, nav_state_t *state, critical_section_t *lock) {
     if (!line || !state || !lock) {
         return;
     }

     critical_section_enter_blocking(lock);

     if (strncmp(line, "NAV:TURN:", 9) == 0) {
         strncpy(state->maneuver, line + 9, sizeof state->maneuver);
         state->maneuver[sizeof state->maneuver - 1] = '\0';
     } else if (strncmp(line, "NAV:DIST:", 9) == 0) {
         uint32_t m = (uint32_t)strtoul(line + 9, NULL, 10);
         state->distance_m = m;
     } else if (strncmp(line, "NAV:STREET:", 11) == 0) {
         strncpy(state->street, line + 11, sizeof state->street);
         state->street[sizeof state->street - 1] = '\0';
     } else if (strncmp(line, "NAV:STATUS:", 11) == 0) {
         const char *s = line + 11;
         if (strcmp(s, "routing") == 0) {
             state->status = NAV_STATUS_ROUTING;
         } else if (strcmp(s, "recalculating") == 0) {
             state->status = NAV_STATUS_RECALCULATING;
         } else {
             state->status = NAV_STATUS_NO_ROUTE;
         }
     } else if (strncmp(line, "NAV:ETA:", 8) == 0) {
         uint32_t t = (uint32_t)strtoul(line + 8, NULL, 10);
         state->eta_unix = t;
     } else if (strncmp(line, "NAV:REMAIN:", 11) == 0) {
         uint32_t m = (uint32_t)strtoul(line + 11, NULL, 10);
         state->remain_m = m;
     } else if (strncmp(line, "BT:TRACK:", 9) == 0) {
         strncpy(state->media, line + 9, sizeof state->media);
         state->media[sizeof state->media - 1] = '\0';
     } else if (strncmp(line, "BT:CALL:", 8) == 0) {
         strncpy(state->caller, line + 8, sizeof state->caller);
         state->caller[sizeof state->caller - 1] = '\0';
         state->call_active = true;
     } else if (strncmp(line, "BT:CALLEND", 10) == 0) {
         state->call_active = false;
         state->caller[0] = '\0';
     }

     critical_section_exit(lock);
 }

 static void feed_char(char c, char *buf, size_t *idx, nav_state_t *state, critical_section_t *lock) {
     if (c == '\r') {
         return;
     }
     if (c == '\n') {
         buf[*idx] = '\0';
         if (*idx > 0) {
             handle_line(buf, state, lock);
         }
         *idx = 0;
         return;
     }

     if (*idx + 1 < LINE_BUF_LEN) {
         buf[*idx] = c;
         (*idx)++;
     }
 }

 void serial_parser_init(void) {
     // Initialise USB stdio (Navit side) and UART1 (Bluetooth).
     stdio_init_all();

     uart_init(BT_UART, BT_UART_BAUD);
     gpio_set_function(BT_UART_TX_PIN, GPIO_FUNC_UART);
     gpio_set_function(BT_UART_RX_PIN, GPIO_FUNC_UART);
 }

 void serial_parser_poll(nav_state_t *state, critical_section_t *lock) {
     static char usb_buf[LINE_BUF_LEN];
     static size_t usb_idx = 0;
     static char bt_buf[LINE_BUF_LEN];
     static size_t bt_idx = 0;

     int ch;
     // USB CDC (non-blocking).
     while ((ch = getchar_timeout_us(0)) != PICO_ERROR_TIMEOUT) {
         feed_char((char)ch, usb_buf, &usb_idx, state, lock);
     }

     // Bluetooth UART.
     while (uart_is_readable(BT_UART)) {
         char c = (char)uart_getc(BT_UART);
         feed_char(c, bt_buf, &bt_idx, state, lock);
     }
 }

