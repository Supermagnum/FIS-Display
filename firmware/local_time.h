#ifndef LOCAL_TIME_H
#define LOCAL_TIME_H

#include <stdbool.h>
#include <stddef.h>

#include "tz_table.h"

/* Parse UTC time from ISO8601 "YYYY-MM-DDTHH:MM:SSZ". Returns true on success. */
bool local_time_parse_iso8601_utc(const char *str, int *year, int *month, int *day,
                                  int *hour, int *min, int *sec);

/* Convert UTC (from Navit) and coordinates to local time string for FIS.
 * Writes "HH:MM" to out_buf (e.g. "14:32"), max buf_len bytes.
 * Uses tz bbox lookup and DST from rule. Returns true if conversion succeeded. */
bool local_time_format_fis(const char *utc_iso8601, tz_coord_t lat_centideg, tz_coord_t lon_centideg,
                           char *out_buf, size_t buf_len);

/* Optional: format with date for line 2, e.g. "14.03.26" (DD.MM.YY). */
void local_time_format_date_short(int year, int month, int day, char *out_buf, size_t buf_len);

/* Convert Unix timestamp (UTC) to local time and format as "HH:MM" or "ARR14:32" for FIS.
 * Uses position for timezone. Returns true on success. */
bool local_time_format_eta(uint32_t unix_ts, tz_coord_t lat_centideg, tz_coord_t lon_centideg,
                           char *out_buf, size_t buf_len);

/* Parse UTC from ISO8601, convert to local using position; fill year/month/day/hour/min/sec.
 * Returns true on success. Used e.g. for OEM CAN mDiagnose_1 (cluster time). */
bool local_time_get_local_components(const char *utc_iso8601, tz_coord_t lat_centideg, tz_coord_t lon_centideg,
                                     int *year, int *month, int *day, int *hour, int *min, int *sec);

#endif
