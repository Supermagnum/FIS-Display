#ifndef FIS_CAN_OEM_H
#define FIS_CAN_OEM_H

#include "fis_config.h"
#include "nav_state.h"
#include <stdbool.h>

/* PQ46/Komfort-CAN OEM message IDs (VW Passat B6). */
#define FIS_CAN_OEM_ID_MDIAGNOSE_1  0x7D0u  /* Date/time from radio; we send from GPS. */
#define FIS_CAN_OEM_ID_MEINHEITEN   0x60Eu  /* Display format (24h/12h, EU/US, units). */

/* Call every main-loop iteration when CAN is enabled. Sends mDiagnose_1 and mEinheiten
 * at 1000 ms interval when GPS time and position are available. */
void fis_can_oem_tick(const nav_state_t *state, const fis_config_t *config);

#endif
