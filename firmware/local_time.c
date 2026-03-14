/*
 * Parse Navit position_time_iso8601 (UTC), look up timezone by position, convert to local, format for FIS.
 */

#include "local_time.h"
#include "tz_lookup.h"
#include <stdlib.h>
#include <stdio.h>
#include <stdint.h>

static int is_leap(int year) {
    return (year % 4 == 0 && year % 100 != 0) || (year % 400 == 0) ? 1 : 0;
}
static int year_days(int y) { return 365 + is_leap(y); }

/* Unix timestamp to UTC (y, m, d, h, min, s). No use of time.h. */
static void unix_to_utc(uint32_t unix_ts, int *y, int *m, int *d, int *h, int *min, int *s) {
    uint32_t sec = unix_ts % 86400u;
    uint32_t days = unix_ts / 86400u;
    *h = (int)(sec / 3600u);
    *min = (int)((sec % 3600u) / 60u);
    *s = (int)(sec % 60u);

    int d_rem = (int)days;
    int yr = 1970;
    while (d_rem >= year_days(yr)) {
        d_rem -= year_days(yr);
        yr++;
    }
    *y = yr;
    int feb = is_leap(yr) ? 29 : 28;
    int md[] = { 31, feb, 31, 30, 31, 30, 31, 31, 30, 31, 30, 31 };
    int mo = 0;
    while (mo < 12 && d_rem >= md[mo]) {
        d_rem -= md[mo];
        mo++;
    }
    *m = mo + 1;
    *d = d_rem + 1;
}

static bool is_digit(char c) {
    return c >= '0' && c <= '9';
}

static int parse2(const char *p) {
    if (!is_digit(p[0]) || !is_digit(p[1])) return -1;
    return (p[0] - '0') * 10 + (p[1] - '0');
}

static int parse4(const char *p) {
    if (!is_digit(p[0]) || !is_digit(p[1]) || !is_digit(p[2]) || !is_digit(p[3])) return -1;
    return (p[0]-'0')*1000 + (p[1]-'0')*100 + (p[2]-'0')*10 + (p[3]-'0');
}

bool local_time_parse_iso8601_utc(const char *str, int *year, int *month, int *day,
                                  int *hour, int *min, int *sec) {
    /* Expect YYYY-MM-DDTHH:MM:SSZ (min 20 chars) */
    if (!str || str[4] != '-' || str[7] != '-' || str[10] != 'T' ||
        str[13] != ':' || str[16] != ':' || (str[19] != 'Z' && str[19] != '\0')) {
        return false;
    }
    int y = parse4(str);
    int m = parse2(str + 5);
    int d = parse2(str + 8);
    int h = parse2(str + 11);
    int mi = parse2(str + 14);
    int s = parse2(str + 17);
    if (y < 2000 || y > 2100 || m < 1 || m > 12 || d < 1 || d > 31 ||
        h > 23 || mi > 59 || s > 59) {
        return false;
    }
    *year = y;
    *month = m;
    *day = d;
    *hour = h;
    *min = mi;
    *sec = s;
    return true;
}

bool local_time_format_fis(const char *utc_iso8601, tz_coord_t lat_centideg, tz_coord_t lon_centideg,
                           char *out_buf, size_t buf_len) {
    if (!out_buf || buf_len < 6) return false;
    int y, mo, d, h, mi, s;
    if (!local_time_parse_iso8601_utc(utc_iso8601, &y, &mo, &d, &h, &mi, &s)) {
        return false;
    }
    int ri = tz_lookup_rule_idx(lat_centideg, lon_centideg);
    if (ri < 0) {
        /* No zone: show UTC as fallback */
        (void)snprintf(out_buf, buf_len, "%02d:%02d", h, mi);
        return true;
    }
    const tz_rule_t *rule = tz_get_rule((uint8_t)ri);
    if (!rule) return false;
    int ly, lm, ld, lh, lmin, ls;
    tz_utc_to_local(rule, y, mo, d, h, mi, s, &ly, &lm, &ld, &lh, &lmin, &ls);
    (void)snprintf(out_buf, buf_len, "%02d:%02d", lh, lmin);
    return true;
}

void local_time_format_date_short(int year, int month, int day, char *out_buf, size_t buf_len) {
    if (!out_buf || buf_len < 9) return;
    int yy = year % 100;
    (void)snprintf(out_buf, buf_len, "%02d.%02d.%02d", day, month, yy);
}

bool local_time_format_eta(uint32_t unix_ts, tz_coord_t lat_centideg, tz_coord_t lon_centideg,
                           char *out_buf, size_t buf_len) {
    if (!out_buf || buf_len < 9) return false;
    int y, mo, d, h, mi, s;
    unix_to_utc(unix_ts, &y, &mo, &d, &h, &mi, &s);
    int ri = tz_lookup_rule_idx(lat_centideg, lon_centideg);
    if (ri < 0) {
        (void)snprintf(out_buf, buf_len, "%02d:%02d", h, mi);
        return true;
    }
    const tz_rule_t *rule = tz_get_rule((uint8_t)ri);
    if (!rule) return false;
    int ly, lm, ld, lh, lmin, ls;
    tz_utc_to_local(rule, y, mo, d, h, mi, s, &ly, &lm, &ld, &lh, &lmin, &ls);
    (void)snprintf(out_buf, buf_len, "ARR%02d:%02d", lh, lmin);
    return true;
}
