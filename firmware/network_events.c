#include "network_events.h"

#include <string.h>


static void copy_string(
    char *destination,
    const char *source,
    size_t destination_size)
{
    if (destination_size == 0)
    {
        return;
    }

    strncpy(
        destination,
        source,
        destination_size - 1
    );

    destination[destination_size - 1] = '\0';
}


void network_events_init(
    network_events_t *events)
{
    memset(
        events,
        0,
        sizeof(*events)
    );

    events->current_channel = -1;
    events->previous_channel = -1;
}


void network_events_begin_sample(
    network_events_t *events)
{
    events->bssid_changed = false;
    events->channel_changed = false;
    events->gateway_changed = false;

    events->weak_signal_event = false;
    events->high_latency_event = false;
}


void network_events_update_ap(
    network_events_t *events,
    const char *bssid,
    int channel,
    uint64_t now_ms)
{
    if (bssid == NULL ||
        bssid[0] == '\0')
    {
        return;
    }


    /*
     * First AP becomes our baseline.
     */
    if (!events->ap_initialized)
    {
        copy_string(
            events->current_bssid,
            bssid,
            sizeof(events->current_bssid)
        );

        events->current_channel =
            channel;

        events->ap_initialized =
            true;

        return;
    }


    /*
     * BSSID changed.
     */
    if (strcmp(
            events->current_bssid,
            bssid) != 0)
    {
        copy_string(
            events->previous_bssid,
            events->current_bssid,
            sizeof(events->previous_bssid)
        );

        copy_string(
            events->current_bssid,
            bssid,
            sizeof(events->current_bssid)
        );

        events->bssid_changed =
            true;

        events->bssid_change_count++;

        events->last_bssid_change_ms =
            now_ms;
    }


    /*
     * Wi-Fi channel changed.
     */
    if (events->current_channel != channel)
    {
        events->previous_channel =
            events->current_channel;

        events->current_channel =
            channel;

        events->channel_changed =
            true;

        events->channel_change_count++;

        events->last_channel_change_ms =
            now_ms;
    }
}


void network_events_update_gateway(
    network_events_t *events,
    const char *gateway,
    uint64_t now_ms)
{
    if (gateway == NULL ||
        gateway[0] == '\0')
    {
        return;
    }


    /*
     * First gateway establishes baseline.
     */
    if (!events->gateway_initialized)
    {
        copy_string(
            events->current_gateway,
            gateway,
            sizeof(events->current_gateway)
        );

        events->gateway_initialized =
            true;

        return;
    }


    /*
     * Same gateway.
     */
    if (strcmp(
            events->current_gateway,
            gateway) == 0)
    {
        return;
    }


    /*
     * Gateway changed.
     */
    copy_string(
        events->previous_gateway,
        events->current_gateway,
        sizeof(events->previous_gateway)
    );

    copy_string(
        events->current_gateway,
        gateway,
        sizeof(events->current_gateway)
    );

    events->gateway_changed =
        true;

    events->gateway_change_count++;

    events->last_gateway_change_ms =
        now_ms;
}


void network_events_update_signal(
    network_events_t *events,
    int rssi,
    int weak_threshold_dbm,
    uint64_t now_ms)
{
    bool weak =
        rssi <= weak_threshold_dbm;


    /*
     * Network has just entered weak-signal state.
     */
    if (weak &&
        !events->weak_signal_active)
    {
        events->weak_signal_active =
            true;

        events->weak_signal_event =
            true;

        events->weak_signal_event_count++;

        events->last_weak_signal_ms =
            now_ms;

        return;
    }


    /*
     * Signal has recovered.
     */
    if (!weak)
    {
        events->weak_signal_active =
            false;
    }
}


void network_events_update_latency(
    network_events_t *events,
    int rtt_ms,
    int high_latency_threshold_ms,
    uint64_t now_ms)
{
    /*
     * A failed ping is handled separately
     * as packet loss/outage.
     */
    if (rtt_ms < 0)
    {
        events->high_latency_active =
            false;

        return;
    }


    bool high_latency =
        rtt_ms >= high_latency_threshold_ms;


    /*
     * RTT has just crossed into high-latency state.
     */
    if (high_latency &&
        !events->high_latency_active)
    {
        events->high_latency_active =
            true;

        events->high_latency_event =
            true;

        events->high_latency_event_count++;

        events->last_high_latency_ms =
            now_ms;

        return;
    }


    /*
     * Latency recovered.
     */
    if (!high_latency)
    {
        events->high_latency_active =
            false;
    }
}
