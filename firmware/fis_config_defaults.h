#ifndef FIS_CONFIG_DEFAULTS_H
#define FIS_CONFIG_DEFAULTS_H

/* Build-time defaults for runtime config. Override here if needed; these are
 * applied by fis_config_set_defaults() and can be changed at runtime via
 * serial CFG:* messages. */

/* CAN bus support: 0 = disabled (default), 1 = enabled.
 * Must be disabled by default. Enable only when external CAN hardware
 * (MCP2515 + MCP2551, 100 kbit/s for B6 comfort/infotainment) is fitted. */
#define FIS_CAN_ENABLED_DEFAULT  0

/* Clock screen toggles (default on). */
#define FIS_SHOW_CLOCK_DEFAULT   1
#define FIS_SHOW_ETA_DEFAULT     1
#define FIS_SHOW_COMPASS_DEFAULT 1
#define FIS_SHOW_REMAIN_DEFAULT  1

#endif
