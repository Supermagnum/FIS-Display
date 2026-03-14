#ifndef TZ_TABLE_H
#define TZ_TABLE_H

#include <stdint.h>

/* Coordinates in centidegrees (degrees * 100). e.g. 5991 = 59.91 deg N */
typedef int16_t tz_coord_t;

/* DST rule: week 1=first..5=last, dow 0=Sun..6=Sat. Transition at 01:00 UTC.
 * If start_month == 0 then no DST (fixed offset only). */
typedef struct {
    int8_t  std_offset_quarters;  /* UTC offset in winter (quarter-hours), e.g. 4 = +1h */
    int8_t  dst_offset_quarters; /* UTC offset in summer */
    uint8_t start_month;         /* 1-12, or 0 = no DST */
    uint8_t start_week;          /* 1-5 (5 = last), 0 = unused */
    uint8_t start_dow;           /* 0-6 Sunday-Saturday */
    uint8_t end_month;
    uint8_t end_week;
    uint8_t end_dow;
} tz_rule_t;

/* Bounding box: first match in table order wins. */
typedef struct {
    tz_coord_t lat_min;
    tz_coord_t lat_max;
    tz_coord_t lon_min;
    tz_coord_t lon_max;
    uint8_t    rule_idx;
} tz_bbox_t;

/* Tables stored in flash (const). Defined in tz_table.c */
extern const tz_rule_t tz_rules[];
extern const uint8_t   tz_rule_count;
extern const tz_bbox_t tz_bboxes[];
extern const uint16_t  tz_bbox_count;

#endif
