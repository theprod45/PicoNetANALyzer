#include "network_events.h"

#include <string.h>


void network_events_init(
    network_events_t *events)
{
    memset(
        events,
        0,
        sizeof(*events)
    );
}


bool network_events_update_bssid(
    network_events_t *events,
    const char *new_bssid,
    uint64_t now_ms)
{
    /*
     * No valid BSSID supplied.
     */
    if (new_bssid == NULL ||
        new_bssid[0] == '\0')
    {
        events->bssid_changed = false;

        return false;
    }


    /*
     * First valid BSSID seen after boot.
     *
     * This establishes the baseline and
     * should NOT count as a change.
     */
    if (!events->bssid_initialized)
    {
        strncpy(
            events->current_bssid,
            new_bssid,
            BSSID_MAX_LEN - 1
        );

        events->current_bssid[
            BSSID_MAX_LEN - 1
        ] = '\0';

        events->bssid_initialized = true;
        events->bssid_changed = false;

        return false;
    }


    /*
     * Same AP as before.
     */
    if (strcmp(
            events->current_bssid,
            new_bssid) == 0)
    {
        events->bssid_changed = false;

        return false;
    }


    /*
     * BSSID changed.
     *
     * Save old AP.
     */
    strncpy(
        events->previous_bssid,
        events->current_bssid,
        BSSID_MAX_LEN - 1
    );

    events->previous_bssid[
        BSSID_MAX_LEN - 1
    ] = '\0';


    /*
     * Save new AP.
     */
    strncpy(
        events->current_bssid,
        new_bssid,
        BSSID_MAX_LEN - 1
    );

    events->current_bssid[
        BSSID_MAX_LEN - 1
    ] = '\0';


    /*
     * Record event.
     */
    events->bssid_change_count++;

    events->last_bssid_change_ms =
        now_ms;

    events->bssid_changed = true;

    return true;
}
