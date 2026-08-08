#ifndef NETWORK_EVENTS_H
#define NETWORK_EVENTS_H

#include <stdbool.h>
#include <stdint.h>

#define BSSID_MAX_LEN 32

typedef struct
{
    /*
     * Have we seen our first valid BSSID yet?
     */
    bool bssid_initialized;

    /*
     * True only for the sample where a
     * BSSID change was detected.
     */
    bool bssid_changed;

    /*
     * Current access point.
     */
    char current_bssid[BSSID_MAX_LEN];

    /*
     * Access point used before the most
     * recent change.
     */
    char previous_bssid[BSSID_MAX_LEN];

    /*
     * Number of BSSID changes observed
     * since boot.
     */
    uint32_t bssid_change_count;

    /*
     * Device uptime when the most recent
     * BSSID change occurred.
     */
    uint64_t last_bssid_change_ms;

} network_events_t;


/*
 * Initialize network event tracking.
 */
void network_events_init(
    network_events_t *events
);


/*
 * Feed the latest BSSID into the event detector.
 *
 * Returns:
 *
 * true  -> BSSID changed
 * false -> no change
 */
bool network_events_update_bssid(
    network_events_t *events,
    const char *new_bssid,
    uint64_t now_ms
);

#endif
