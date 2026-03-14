#ifndef FIS_CONFIG_H
#define FIS_CONFIG_H

#include <stdbool.h>

/* Feature toggles for FIS clock screen. All default true. Set via serial CFG:* messages. */
typedef struct {
    bool show_clock;   /* Show clock (time from GPS, set automatically) */
    bool show_eta;     /* Show ETA (estimated arrival in local time) on clock line 2 */
    bool show_compass; /* Show heading as compass (N, NE, ...) on clock line 2 */
    bool show_remain;  /* Show remaining distance to destination on clock line 2 */
} fis_config_t;

void fis_config_set_defaults(fis_config_t *c);

#endif
