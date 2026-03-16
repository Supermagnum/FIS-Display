/*
 * Software CAN 2.0A driver for MCP2561 on GPIO 11 (TX) and GPIO 12 (RX).
 * Bit rate: 100 kbit/s (10 us per bit). B6 comfort/infotainment CAN.
 */

#include "fis_can.h"
#include "hardware/gpio.h"
#include "pico/stdlib.h"
#include "pico/time.h"

#include <stdbool.h>
#include <stddef.h>
#include <string.h>

/* 100 kbit/s: 10 us per bit. Sample point ~80% for RX. */
#define FIS_CAN_BIT_US       10u
#define FIS_CAN_SAMPLE_US    8u

/* Max bits after stuffing: SOF(1) + arb(11+1+1+1+4) + data(64) + CRC(15) + delim(1) + ~20 stuff */
#define FIS_CAN_TX_BUF_BITS  128u

static bool s_can_initialized;

/* CRC-15 CAN polynomial: x^15 + x^14 + x^10 + x^8 + x^7 + x^4 + x^3 + 1  =>  0x4599 */
#define FIS_CAN_CRC_POLY  0x4599u

static uint16_t fis_can_crc15_bit(uint16_t crc, bool bit) {
    uint16_t in = (crc << 1) | (bit ? 1u : 0u);
    if (in & 0x8000u)
        in ^= FIS_CAN_CRC_POLY;
    return in & 0x7FFFu;
}

/* Push one unstuffed bit into stream; update crc if non-NULL. *n_bits is count of unstuffed bits so far. */
static void push_bit(unsigned char *buf, unsigned int *n_bits, bool bit, uint16_t *crc) {
    unsigned int byte_ix = *n_bits / 8u;
    unsigned int bit_ix = *n_bits % 8u;
    if (bit)
        buf[byte_ix] |= (unsigned char)(1u << bit_ix);
    else
        buf[byte_ix] &= (unsigned char)~(1u << bit_ix);
    if (crc)
        *crc = fis_can_crc15_bit(*crc, bit);
    (*n_bits)++;
}

/* Append unstuffed bits to buf (LSB first per byte), update crc. *n_bits is total unstuffed. */
static void push_bits(unsigned char *buf, unsigned int *n_bits, uint32_t value, unsigned int num_bits, uint16_t *crc) {
    for (unsigned int i = 0; i < num_bits; i++) {
        push_bit(buf, n_bits, (value >> i) & 1u, crc);
    }
}

/* Build TX bit stream with stuffing. Input: unstuffed bits in buf (num_bits). Output: stuffed into out_buf, return count. */
static unsigned int fis_can_stuff(const unsigned char *buf, unsigned int num_bits, unsigned char *out_buf) {
    unsigned int out_bits = 0;
    unsigned int run = 0;
    bool last = false;
    for (unsigned int i = 0; i < num_bits; i++) {
        unsigned int byte_ix = i / 8u;
        unsigned int bit_ix = i % 8u;
        bool b = (buf[byte_ix] >> bit_ix) & 1u;
        if (b == last) {
            run++;
            if (run == 5) {
                /* Insert stuff bit (opposite). */
                unsigned int ob = out_bits / 8u;
                unsigned int oi = out_bits % 8u;
                out_buf[ob] &= (unsigned char)~(1u << oi);
                out_buf[ob] |= (unsigned char)((!b) ? (1u << oi) : 0u);
                out_bits++;
                run = 0;
                last = !b;
            }
        } else {
            run = 1;
            last = b;
        }
        unsigned int ob = out_bits / 8u;
        unsigned int oi = out_bits % 8u;
        if (b)
            out_buf[ob] |= (unsigned char)(1u << oi);
        else
            out_buf[ob] &= (unsigned char)~(1u << oi);
        out_bits++;
        last = b;
    }
    return out_bits;
}

/* Dominant = low on bus (MCP2561 TXD low); recessive = high. */
static void fis_can_tx_bit(bool dominant) {
    gpio_put(FIS_CAN_PIN_TX, dominant ? 0 : 1);
    busy_wait_us(FIS_CAN_BIT_US);
}

static bool fis_can_rx_bit(void) {
    busy_wait_us(FIS_CAN_SAMPLE_US);
    bool v = gpio_get(FIS_CAN_PIN_RX) != 0;
    busy_wait_us(FIS_CAN_BIT_US - FIS_CAN_SAMPLE_US);
    return v;
}


void fis_can_init(void) {
    if (s_can_initialized)
        return;
    gpio_init(FIS_CAN_PIN_TX);
    gpio_init(FIS_CAN_PIN_RX);
    gpio_set_dir(FIS_CAN_PIN_TX, true);
    gpio_set_dir(FIS_CAN_PIN_RX, false);
    gpio_put(FIS_CAN_PIN_TX, 0);
    s_can_initialized = true;
}

bool fis_can_send(const fis_can_frame_t *frame) {
    if (!frame || frame->dlc > FIS_CAN_MAX_DLC)
        return false;
    if (!s_can_initialized)
        return false;

    unsigned char unstuffed[16];
    memset(unstuffed, 0, sizeof unstuffed);
    unsigned int n_bits = 0;
    uint16_t crc = 0;

    push_bit(unstuffed, &n_bits, false, &crc);  /* SOF dominant */
    push_bits(unstuffed, &n_bits, frame->id & FIS_CAN_STD_ID_MASK, 11, &crc);
    push_bit(unstuffed, &n_bits, frame->rtr, &crc);
    push_bit(unstuffed, &n_bits, false, &crc);  /* IDE = 0 standard */
    push_bit(unstuffed, &n_bits, false, &crc);  /* r0 */
    push_bits(unstuffed, &n_bits, frame->dlc & 0xFu, 4, &crc);
    for (unsigned int i = 0; i < frame->dlc; i++)
        push_bits(unstuffed, &n_bits, frame->data[i], 8, &crc);

    /* CRC and delimiter: append 15-bit CRC (LSB first) then 1 recessive. CRC not included in CRC calc. */
    for (unsigned int i = 0; i < 15u; i++)
        push_bit(unstuffed, &n_bits, (crc >> i) & 1u, NULL);
    push_bit(unstuffed, &n_bits, true, NULL);   /* CRC delimiter recessive */

    unsigned char stuffed[20];
    memset(stuffed, 0, sizeof stuffed);
    unsigned int stuffed_bits = fis_can_stuff(unstuffed, n_bits, stuffed);

    /* Send stuffed frame. */
    for (unsigned int i = 0; i < stuffed_bits; i++) {
        unsigned int ob = i / 8u;
        unsigned int oi = i % 8u;
        bool b = (stuffed[ob] >> oi) & 1u;
        fis_can_tx_bit(!b);  /* b=1 recessive, b=0 dominant; tx_bit(dominant) drives low for dominant */
    }

    /* ACK slot: we send recessive; receiver may drive dominant. */
    fis_can_tx_bit(true);
    /* ACK delimiter + EOF (7 recessive). */
    for (int i = 0; i < 8; i++)
        fis_can_tx_bit(true);

    return true;
}

/* Read one bit with destuffing. Returns false on timeout or error. */
static bool fis_can_rx_bit_destuff(bool *bit_out, unsigned int *run_count, bool *last_bit) {
    bool b = fis_can_rx_bit();
    if (b == *last_bit) {
        (*run_count)++;
        if (*run_count == 5) {
            /* Next bit is stuff bit - skip it. */
            (void)fis_can_rx_bit();
            *run_count = 0;
            *last_bit = !b;
        }
    } else {
        *run_count = 1;
        *last_bit = b;
    }
    *bit_out = b;
    return true;
}

#define FIS_CAN_RX_SOF_TIMEOUT_US  2000u  /* 2 ms; return false if no SOF */

bool fis_can_receive(fis_can_frame_t *out) {
    if (!out || !s_can_initialized)
        return false;

    /* Wait for idle (recessive), then SOF (dominant), with timeout. */
    uint32_t t0 = time_us_32();
    while (gpio_get(FIS_CAN_PIN_RX) == 0) {
        if ((time_us_32() - t0) > FIS_CAN_RX_SOF_TIMEOUT_US)
            return false;
        busy_wait_us(1);
    }
    while (gpio_get(FIS_CAN_PIN_RX) != 0) {
        if ((time_us_32() - t0) > FIS_CAN_RX_SOF_TIMEOUT_US)
            return false;
        busy_wait_us(1);
    }
    /* Now at SOF (dominant). Align to first bit of identifier. */
    busy_wait_us(FIS_CAN_BIT_US);

    unsigned int run = 0;
    bool last = false;
    /* CRC is over SOF through last data bit. We synced on SOF (dominant = 0). */
    uint16_t crc_calc = fis_can_crc15_bit(0, false);

#define RX_BIT(b) do { \
    bool _b; \
    if (!fis_can_rx_bit_destuff(&_b, &run, &last)) return false; \
    crc_calc = fis_can_crc15_bit(crc_calc, _b); \
    (b) = _b; \
} while (0)
#define RX_BIT_NO_CRC(b) do { \
    if (!fis_can_rx_bit_destuff(&(b), &run, &last)) return false; \
} while (0)

    /* After SOF we have 11 ID, 1 RTR, 1 IDE, 1 r0, 4 DLC. */
    uint16_t id = 0;
    bool rtr, ide, r0;
    uint8_t dlc = 0;
    for (int i = 0; i < 11; i++) {
        bool b;
        RX_BIT(b);
        if (b) id |= (1u << i);
    }
    RX_BIT(rtr);
    RX_BIT(ide);
    RX_BIT(r0);
    for (int i = 0; i < 4; i++) {
        bool b;
        RX_BIT(b);
        if (b) dlc |= (uint8_t)(1u << i);
    }
    if (dlc > FIS_CAN_MAX_DLC)
        return false;

    uint8_t data[FIS_CAN_MAX_DLC];
    memset(data, 0, sizeof data);
    for (unsigned int j = 0; j < dlc; j++) {
        for (int i = 0; i < 8; i++) {
            bool b;
            RX_BIT(b);
            if (b) data[j] |= (uint8_t)(1u << i);
        }
    }

    uint16_t crc_rx = 0;
    for (int i = 0; i < 15; i++) {
        bool b;
        RX_BIT_NO_CRC(b);
        if (b) crc_rx |= (uint16_t)(1u << i);
    }
    bool crc_delim;
    RX_BIT_NO_CRC(crc_delim);
    if (!crc_delim)
        return false;

    if (crc_calc != crc_rx)
        return false;

    /* Send ACK: drive dominant for one bit time. */
    gpio_put(FIS_CAN_PIN_TX, 0);
    busy_wait_us(FIS_CAN_BIT_US);
    gpio_put(FIS_CAN_PIN_TX, 1);
    /* Consume ACK delimiter + EOF (8 bits). */
    for (int i = 0; i < 8; i++)
        (void)fis_can_rx_bit();

    out->id = id & FIS_CAN_STD_ID_MASK;
    out->rtr = rtr;
    out->dlc = dlc;
    memcpy(out->data, data, (size_t)dlc);
    return true;
#undef RX_BIT_NO_CRC
#undef RX_BIT
}

void fis_can_ensure_init(void) {
    if (!s_can_initialized)
        fis_can_init();
}

void fis_can_poll(const fis_config_t *config) {
    if (!config || !config->can_enabled)
        return;
    fis_can_ensure_init();
    fis_can_frame_t frame;
    while (fis_can_receive(&frame)) {
        (void)frame;
    }
}
