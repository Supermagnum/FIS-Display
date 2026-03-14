 #ifndef NAV_STATE_H
 #define NAV_STATE_H

 #include <stdint.h>
 #include <stdbool.h>

 typedef enum {
     NAV_STATUS_NO_ROUTE = 0,
     NAV_STATUS_ROUTING,
     NAV_STATUS_RECALCULATING
 } nav_status_t;

 /* Position in centidegrees (degrees * 100). TZ_COORD_INVALID when unknown. */
 #define NAV_POS_INVALID  (-32768)

 typedef struct {
     nav_status_t status;
     uint32_t     distance_m;
     uint32_t     remain_m;
     uint32_t     eta_unix;

     bool         call_active;

     char         maneuver[32];
     char         street[32];
     char         media[32];
     char         caller[32];

     /* From Navit: position_time_iso8601 (UTC), position_coord_geo, position_direction, destination_length */
     char         position_time_iso8601[24];
     int16_t      lat_centideg;   /* NAV_POS_INVALID when unknown */
     int16_t      lon_centideg;
     uint16_t     heading_deg;   /* 0-360; NAV_HEADING_INVALID when unknown */
 } nav_state_t;

 #define NAV_HEADING_INVALID  (361u)

 #endif

