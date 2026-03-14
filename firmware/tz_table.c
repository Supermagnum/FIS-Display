/*
 * Compact timezone bounding-box and DST rules for Pico. Stored in flash.
 * No network, no OS tz database. First matching bbox wins; DST computed from rule.
 */

#include "tz_table.h"

/* Rules: offset in quarter-hours (4 = +1h). EU: last Sun Mar 01:00 UTC, last Sun Oct 01:00 UTC. */
const tz_rule_t tz_rules[] = {
    /* 0: UTC+0, no DST (Iceland, etc.) */
    { 0, 0, 0, 0, 0, 0, 0, 0 },
    /* 1: UK/Ireland: GMT, BST last Sun Mar - last Sun Oct */
    { 0, 4, 3, 5, 0, 10, 5, 0 },
    /* 2: UTC+1, no DST */
    { 4, 4, 0, 0, 0, 0, 0, 0 },
    /* 3: CET: UTC+1, CEST last Sun Mar - last Sun Oct (most of EU) */
    { 4, 8, 3, 5, 0, 10, 5, 0 },
    /* 4: UTC+2, no DST */
    { 8, 8, 0, 0, 0, 0, 0, 0 },
    /* 5: EET: UTC+2, EEST last Sun Mar - last Sun Oct (Finland, Greece, etc.) */
    { 8, 12, 3, 5, 0, 10, 5, 0 },
    /* 6: UTC+3, no DST (Moscow, etc.) */
    { 12, 12, 0, 0, 0, 0, 0, 0 },
};

/* Bboxes in centidegrees. First match wins. Order: more specific first. */
const tz_bbox_t tz_bboxes[] = {
    /* UK + Ireland */
    { 4960, 6090, -1060, 180, 1 },
    /* Iceland */
    { 6300, 6700, -2400, -1300, 0 },
    /* Finland */
    { 5980, 7100, 2000, 3200, 5 },
    /* Greece, Bulgaria, Romania (EET) */
    { 3500, 4400, 1900, 3000, 5 },
    /* Poland, E Europe EET */
    { 4900, 5500, 1400, 2500, 5 },
    /* Norway, Sweden, Denmark, Germany, France, Spain, Italy, etc. (CET) */
    { 3500, 7200, -1100, 4000, 3 },
};

const uint8_t   tz_rule_count  = (uint8_t)(sizeof(tz_rules) / sizeof(tz_rules[0]));
const uint16_t  tz_bbox_count   = (uint16_t)(sizeof(tz_bboxes) / sizeof(tz_bboxes[0]));
