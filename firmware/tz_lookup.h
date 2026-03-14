#ifndef TZ_LOOKUP_H
#define TZ_LOOKUP_H

#include <stdbool.h>
#include <stdint.h>

#include "tz_table.h"

/* Coordinates in centidegrees (degrees * 100). Use TZ_COORD_INVALID when position unknown. */
#define TZ_COORD_INVALID  (-32768)

/* Look up timezone rule index for (lat_centideg, lon_centideg). Returns rule index or -1 if no match. */
int tz_lookup_rule_idx(tz_coord_t lat_c, tz_coord_t lon_c);

/* Get the rule pointer for a rule index. Returns NULL if invalid. */
const tz_rule_t *tz_get_rule(uint8_t rule_idx);

/* Compute whether DST is in effect at given UTC time for the given rule.
 * year/month/day/hour/min/sec are UTC. Returns true if DST active. */
bool tz_is_dst(const tz_rule_t *rule, int year, int month, int day, int hour, int min, int sec);

/* Compute local time from UTC using the rule. UTC and output: year, month, day, hour, min, sec.
 * Day/month/year may roll over. All in range: month 1-12, day 1-31, hour 0-23, min 0-59, sec 0-59. */
void tz_utc_to_local(const tz_rule_t *rule, int utc_y, int utc_m, int utc_d, int utc_h, int utc_min, int utc_s,
                    int *local_y, int *local_m, int *local_d, int *local_h, int *local_min, int *local_s);

#endif
