#include "network_events.h"

#include <string.h>


static void copy_string(
    char *destination,
    const char *source,
    size_t destination_size)
{
    if (
        destination == NULL ||
        source == NULL ||
        destination_size == 0
    )
    {
        return;
    }


    strncpy(
        destination,
        source,
        destination_size - 1
    );


    destination[
        destination_size - 1
    ] = '\0';
}


void network_events_init(
    network_events_t *events)
{
    memset(
        events,
        0,
        sizeof(*events)
    );


    events->current_channel =
        -1;


    events->previous_channel =
        -1;
}


void network_events_begin_sample(
    network_events_t *events)
{
    /*
     * One-cycle state-change flags.
     */

    events->bssid_changed =
        false;

    events->channel_changed =
        false;

    events->gateway_changed =
        false;


    events->weak_signal_event =
        false;

    events->high_latency_event =
        false;

    events->high_jitter_event =
        false;

    events->high_packet_loss_event =
        false;


    events->internet_outage_event =
        false;

    events->internet_recovered_event =
        false;


    events->dns_failure_event =
        false;

    events->dns_recovered_event =
        false;
}


/*
 * ================================================================
 * AP / BSSID / channel
 * ================================================================
 */

void network_events_update_ap(
    network_events_t *events,
    const char *bssid,
    int channel,
    uint64_t now_ms)
{
    if (
        bssid == NULL ||
        bssid[0] == '\0'
    )
    {
        return;
    }


    /*
     * First AP becomes baseline.
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
    if (
        strcmp(
            events->current_bssid,
            bssid
        ) != 0
    )
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
     * Channel changed.
     */
    if (
        events->current_channel !=
        channel
    )
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


/*
 * ================================================================
 * Gateway
 * ================================================================
 */

void network_events_update_gateway(
    network_events_t *events,
    const char *gateway,
    uint64_t now_ms)
{
    if (
        gateway == NULL ||
        gateway[0] == '\0'
    )
    {
        return;
    }


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


    if (
        strcmp(
            events->current_gateway,
            gateway
        ) == 0
    )
    {
        return;
    }


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


/*
 * ================================================================
 * Weak signal
 * ================================================================
 */

void network_events_update_signal(
    network_events_t *events,
    int rssi,
    int weak_threshold_dbm,
    uint64_t now_ms)
{
    bool weak =
        rssi <= weak_threshold_dbm;


    /*
     * Just entered weak-signal condition.
     */
    if (
        weak &&
        !events->weak_signal_active
    )
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
     * Signal recovered.
     */
    if (!weak)
    {
        events->weak_signal_active =
            false;
    }
}


/*
 * ================================================================
 * High latency
 * ================================================================
 */

void network_events_update_latency(
    network_events_t *events,
    int rtt_ms,
    int threshold_ms,
    uint64_t now_ms)
{
    if (rtt_ms < 0)
    {
        events->high_latency_active =
            false;


        return;
    }


    bool high =
        rtt_ms >= threshold_ms;


    if (
        high &&
        !events->high_latency_active
    )
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


    if (!high)
    {
        events->high_latency_active =
            false;
    }
}


/*
 * ================================================================
 * High jitter
 * ================================================================
 */

void network_events_update_jitter(
    network_events_t *events,
    int jitter_ms,
    int threshold_ms,
    uint64_t now_ms)
{
    /*
     * -1 means jitter is unavailable because there
     * weren't two consecutive successful RTT probes.
     */
    if (jitter_ms < 0)
    {
        events->high_jitter_active =
            false;


        return;
    }


    bool high =
        jitter_ms >= threshold_ms;


    /*
     * Generate one event when crossing into the
     * high-jitter state.
     */
    if (
        high &&
        !events->high_jitter_active
    )
    {
        events->high_jitter_active =
            true;


        events->high_jitter_event =
            true;


        events->high_jitter_event_count++;


        events->last_high_jitter_ms =
            now_ms;


        return;
    }


    /*
     * Jitter has recovered.
     */
    if (!high)
    {
        events->high_jitter_active =
            false;
    }
}


/*
 * ================================================================
 * Rolling packet loss
 * ================================================================
 */

void network_events_update_packet_loss(
    network_events_t *events,
    bool probe_success,
    float threshold_pct,
    uint32_t minimum_samples,
    uint64_t now_ms)
{
    /*
     * Store:
     *
     * 0 = success
     * 1 = failure
     */
    uint8_t new_failure =
        probe_success
            ? 0
            : 1;


    /*
     * If the ring buffer is full, remove the value
     * currently occupying this position.
     */
    if (
        events->packet_loss_window_count >=
        PACKET_LOSS_WINDOW_SIZE
    )
    {
        events->packet_loss_window_failures -=
            events->packet_loss_window[
                events->packet_loss_window_index
            ];
    }
    else
    {
        events->packet_loss_window_count++;
    }


    /*
     * Insert newest result.
     */
    events->packet_loss_window[
        events->packet_loss_window_index
    ] = new_failure;


    events->packet_loss_window_failures +=
        new_failure;


    /*
     * Advance circular-buffer position.
     */
    events->packet_loss_window_index =
        (
            events->packet_loss_window_index
            + 1
        )
        %
        PACKET_LOSS_WINDOW_SIZE;


    /*
     * Calculate rolling percentage.
     */
    if (
        events->packet_loss_window_count > 0
    )
    {
        events->current_packet_loss_window_pct =
            (
                (float)
                events->packet_loss_window_failures
                /
                (float)
                events->packet_loss_window_count
            )
            * 100.0f;
    }


    /*
     * Wait until we have enough measurements
     * before raising an alert.
     */
    if (
        events->packet_loss_window_count <
        minimum_samples
    )
    {
        return;
    }


    bool high =
        events->current_packet_loss_window_pct
        >= threshold_pct;


    /*
     * Enter high packet-loss state.
     */
    if (
        high &&
        !events->high_packet_loss_active
    )
    {
        events->high_packet_loss_active =
            true;


        events->high_packet_loss_event =
            true;


        events->high_packet_loss_event_count++;


        events->last_high_packet_loss_ms =
            now_ms;


        return;
    }


    /*
     * Packet loss recovered.
     */
    if (!high)
    {
        events->high_packet_loss_active =
            false;
    }
}


/*
 * ================================================================
 * Internet outage
 * ================================================================
 */

void network_events_update_internet(
    network_events_t *events,
    bool eligible,
    bool internet_success,
    uint32_t failure_threshold,
    uint64_t now_ms)
{
    /*
     * Only classify an Internet outage when:
     *
     * Wi-Fi works AND gateway works.
     *
     * Otherwise the root problem is lower down
     * in the network stack.
     */
    if (!eligible)
    {
        events->internet_failure_streak =
            0;


        return;
    }


    /*
     * Internet working.
     */
    if (internet_success)
    {
        events->internet_failure_streak =
            0;


        /*
         * Recover from an existing outage.
         */
        if (events->internet_outage_active)
        {
            events->internet_outage_active =
                false;


            events->internet_recovered_event =
                true;


            events->last_internet_recovery_ms =
                now_ms;


            events->last_internet_outage_duration_ms =
                now_ms -
                events->internet_outage_start_ms;
        }


        return;
    }


    /*
     * Failed Internet probe.
     */
    events->internet_failure_streak++;


    /*
     * Already in outage state.
     */
    if (events->internet_outage_active)
    {
        return;
    }


    /*
     * Require multiple failures to avoid declaring
     * an outage because one ping was dropped.
     */
    if (
        events->internet_failure_streak >=
        failure_threshold
    )
    {
        events->internet_outage_active =
            true;


        events->internet_outage_event =
            true;


        events->internet_outage_count++;


        events->internet_outage_start_ms =
            now_ms;


        events->last_internet_outage_ms =
            now_ms;
    }
}


/*
 * ================================================================
 * DNS failure / recovery
 * ================================================================
 */

void network_events_update_dns(
    network_events_t *events,
    bool dns_success,
    uint32_t failure_threshold,
    uint64_t now_ms)
{
    /*
     * DNS working.
     */
    if (dns_success)
    {
        events->dns_failure_streak =
            0;


        if (events->dns_failure_active)
        {
            events->dns_failure_active =
                false;


            events->dns_recovered_event =
                true;


            events->last_dns_recovery_ms =
                now_ms;
        }


        return;
    }


    /*
     * Failed DNS lookup.
     */
    events->dns_failure_streak++;


    if (events->dns_failure_active)
    {
        return;
    }


    /*
     * Require consecutive failures.
     */
    if (
        events->dns_failure_streak >=
        failure_threshold
    )
    {
        events->dns_failure_active =
            true;


        events->dns_failure_event =
            true;


        events->dns_failure_count++;


        events->last_dns_failure_ms =
            now_ms;
    }
}
