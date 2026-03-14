/*
 * Timezone lookup by (lat, lon) and DST calculation from rule. No OS or network.
 */

#include "tz_lookup.h"
#include "tz_table.h"

int tz_lookup_rule_idx(tz_coord_t lat_c, tz_coord_t lon_c) {
    if (lat_c == TZ_COORD_INVALID || lon_c == TZ_COORD_INVALID) {
        return -1;
    }
    for (uint16_t i = 0; i < tz_bbox_count; i++) {
        const tz_bbox_t *b = &tz_bboxes[i];
        if (lat_c >= b->lat_min && lat_c <= b->lat_max && lon_c >= b->lon_min && lon_c <= b->lon_max) {
            return (int)b->rule_idx;
        }
    }
    return -1;
}

const tz_rule_t *tz_get_rule(uint8_t rule_idx) {
    if (rule_idx >= tz_rule_count) {
        return NULL;
    }
    return &tz_rules[rule_idx];
}

/* Day of week for given date. 0 = Sunday, 1 = Monday, ... 6 = Saturday. */
static int day_of_week(int year, int month, int day) {
    /* 1970-01-01 is Thursday. Simple day count from then. */
    static const int days_before[] = { 0, 31, 59, 90, 120, 151, 181, 212, 243, 273, 304, 334 };
    int leap = (year % 4 == 0 && year % 100 != 0) || (year % 400 == 0);
    int y = year - 1970;
    int days = y * 365 + (y / 4) - (y / 100) + (y / 400);
    if (month > 2) {
        days += leap;
    }
    days += days_before[month - 1] + (day - 1);
    /* Thursday = 4 in Sun=0 convention: (days + 4) % 7 */
    return (days + 4) % 7;
}

/* Last occurrence of weekday 'dow' (0=Sun) in month (month_len days). day_1_dow = day_of_week(y, m, 1). */
static int last_weekday_in_month(int month_len, int day_1_dow, int dow) {
    int first = 1 + (dow - day_1_dow + 7) % 7;
    /* first is 1..7; month_len >= 28, so first <= month_len always */
    return first + 7 * ((month_len - first) / 7);
}

static int month_days(int year, int month) {
    static const int d[] = { 31, 28, 31, 30, 31, 30, 31, 31, 30, 31, 30, 31 };
    int n = d[month - 1];
    if (month == 2 && ((year % 4 == 0 && year % 100 != 0) || (year % 400 == 0))) {
        n = 29;
    }
    return n;
}

/* Compare two UTC moments (y,m,d,h,min,s). Returns -1 if a < b, 0 if a == b, 1 if a > b. */
static int compare_utc(int ay, int am, int ad, int ah, int amin, int as,
                      int by, int bm, int bd, int bh, int bmin, int bs) {
    if (ay != by) return ay < by ? -1 : 1;
    if (am != bm) return am < bm ? -1 : 1;
    if (ad != bd) return ad < bd ? -1 : 1;
    if (ah != bh) return ah < bh ? -1 : 1;
    if (amin != bmin) return amin < bmin ? -1 : 1;
    if (as != bs) return as < bs ? -1 : 1;
    return 0;
}

bool tz_is_dst(const tz_rule_t *rule, int year, int month, int day, int hour, int min, int sec) {
    if (!rule || rule->start_month == 0) {
        return false;
    }
    int start_d = last_weekday_in_month(month_days(year, rule->start_month),
                                       day_of_week(year, rule->start_month, 1),
                                       (int)rule->start_dow);
    int end_d = last_weekday_in_month(month_days(year, rule->end_month),
                                     day_of_week(year, rule->end_month, 1),
                                     (int)rule->end_dow);
    int c_start = compare_utc(year, month, day, hour, min, sec,
                              year, rule->start_month, start_d, 1, 0, 0);
    int c_end   = compare_utc(year, month, day, hour, min, sec,
                              year, rule->end_month, end_d, 1, 0, 0);
    return c_start >= 0 && c_end < 0;
}

/* Add delta_min minutes to (y,m,d,h,min,s); normalize day/month/year. */
static void add_minutes(int *y, int *m, int *d, int *h, int *min, int *s, int delta_min) {
    int total_sec = (*h * 3600 + *min * 60 + *s) + (delta_min * 60);
    int day_delta = 0;
    while (total_sec < 0) {
        total_sec += 86400;
        day_delta--;
    }
    while (total_sec >= 86400) {
        total_sec -= 86400;
        day_delta++;
    }
    *h = total_sec / 3600;
    *min = (total_sec % 3600) / 60;
    *s = total_sec % 60;
    *d += day_delta;
    while (*d > month_days(*y, *m)) {
        *d -= month_days(*y, *m);
        (*m)++;
        if (*m > 12) {
            *m = 1;
            (*y)++;
        }
    }
    while (*d < 1) {
        (*m)--;
        if (*m < 1) {
            *m = 12;
            (*y)--;
        }
        *d += month_days(*y, *m);
    }
}

void tz_utc_to_local(const tz_rule_t *rule, int utc_y, int utc_m, int utc_d, int utc_h, int utc_min, int utc_s,
                     int *local_y, int *local_m, int *local_d, int *local_h, int *local_min, int *local_s) {
    int offset_quarters = rule->std_offset_quarters;
    if (tz_is_dst(rule, utc_y, utc_m, utc_d, utc_h, utc_min, utc_s)) {
        offset_quarters = rule->dst_offset_quarters;
    }
    *local_y   = utc_y;
    *local_m   = utc_m;
    *local_d   = utc_d;
    *local_h   = utc_h;
    *local_min = utc_min;
    *local_s   = utc_s;
    add_minutes(local_y, local_m, local_d, local_h, local_min, local_s, offset_quarters * 15);
}
