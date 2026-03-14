#include "fis_can.h"

#include <stdbool.h>
#include <stddef.h>

static bool s_can_initialized;

static void fis_can_init_once(void) {
    /* Placeholder for future CAN hardware init (MCP2561 TXD/RXD on GPIO 11/12).
     * No-op until external CAN support is implemented. */
    (void)0;
}

void fis_can_poll(const fis_config_t *config) {
    if (!config || !config->can_enabled)
        return;
    if (!s_can_initialized) {
        fis_can_init_once();
        s_can_initialized = true;
    }
    /* Placeholder: read cluster/vehicle CAN frames when implemented. */
}
