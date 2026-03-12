 #ifndef NAV_STATE_H
 #define NAV_STATE_H

 #include <stdint.h>
 #include <stdbool.h>

 typedef enum {
     NAV_STATUS_NO_ROUTE = 0,
     NAV_STATUS_ROUTING,
     NAV_STATUS_RECALCULATING
 } nav_status_t;

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
 } nav_state_t;

 #endif

