#include <stdio.h>
#include <string.h>
#include <stdlib.h>
#include <stdbool.h>
#include <ctype.h>

#include "pico/stdlib.h"
#include "hardware/uart.h"

#include "config.h"
#include "network_events.h"
#include "telemetry.h"


#define WIZ_UART uart1
#define WIZ_TX_PIN 4
#define WIZ_RX_PIN 5

#define RESPONSE_SIZE 1024


typedef struct
{
    char ssid[64];
    char bssid[32];

    int channel;
    int rssi;

    bool valid;

} wifi_info_t;


/*
 * ================================================================
 * WizFi360 AT command interface
 * ================================================================
 */

/*
 * Send an AT command to the WizFi360.
 *
 * The response is captured internally and is not printed
 * directly to USB.
 */
static bool wizfi_command(
    const char *command,
    char *response,
    size_t response_size,
    uint32_t timeout_ms)
{
    /*
     * Flush any stale bytes.
     */
    while (uart_is_readable(WIZ_UART))
    {
        uart_getc(WIZ_UART);
    }


    response[0] = '\0';


    /*
     * Send command.
     */
    uart_puts(
        WIZ_UART,
        command
    );

    uart_puts(
        WIZ_UART,
        "\r\n"
    );


    absolute_time_t timeout =
        make_timeout_time_ms(
            timeout_ms
        );


    size_t pos = 0;


    while (!time_reached(timeout))
    {
        while (uart_is_readable(WIZ_UART))
        {
            char c =
                uart_getc(WIZ_UART);


            if (pos < response_size - 1)
            {
                response[pos++] = c;
                response[pos] = '\0';
            }


            /*
             * Successful command.
             */
            if (
                strstr(
                    response,
                    "\r\nOK\r\n"
                ) != NULL
                ||
                strstr(
                    response,
                    "\nOK\r\n"
                ) != NULL
            )
            {
                return true;
            }


            /*
             * Failed command.
             */
            if (
                strstr(
                    response,
                    "ERROR"
                ) != NULL
                ||
                strstr(
                    response,
                    "FAIL"
                ) != NULL
            )
            {
                return false;
            }
        }


        tight_loop_contents();
    }


    return false;
}


/*
 * ================================================================
 * Wi-Fi connection
 * ================================================================
 */

static bool connect_wifi(
    char *response,
    size_t response_size)
{
    /*
     * Station mode.
     */
    wizfi_command(
        "AT+CWMODE_CUR=1",
        response,
        response_size,
        2000
    );


    char command[160];


    snprintf(
        command,
        sizeof(command),
        "AT+CWJAP_CUR=\"%s\",\"%s\"",
        WIFI_SSID,
        WIFI_PASSWORD
    );


    return wizfi_command(
        command,
        response,
        response_size,
        15000
    );
}


/*
 * Get:
 *
 * SSID
 * BSSID
 * Wi-Fi channel
 * RSSI
 */
static wifi_info_t get_wifi_info(void)
{
    wifi_info_t info = {0};


    char response[RESPONSE_SIZE];


    if (!wizfi_command(
            "AT+CWJAP_CUR?",
            response,
            sizeof(response),
            2000))
    {
        return info;
    }


    char *line =
        strstr(
            response,
            "+CWJAP_CUR:"
        );


    if (line == NULL)
    {
        return info;
    }


    int result =
        sscanf(
            line,
            "+CWJAP_CUR:\"%63[^\"]\",\"%31[^\"]\",%d,%d",
            info.ssid,
            info.bssid,
            &info.channel,
            &info.rssi
        );


    if (result == 4)
    {
        info.valid = true;
    }


    return info;
}


/*
 * ================================================================
 * Gateway
 * ================================================================
 */

static bool get_gateway(
    char *gateway,
    size_t size)
{
    char response[RESPONSE_SIZE];


    if (!wizfi_command(
            "AT+CIPSTA_CUR?",
            response,
            sizeof(response),
            2000))
    {
        return false;
    }


    const char *prefix =
        "+CIPSTA_CUR:gateway:\"";


    char *start =
        strstr(
            response,
            prefix
        );


    if (start == NULL)
    {
        return false;
    }


    start += strlen(prefix);


    char *end =
        strchr(
            start,
            '"'
        );


    if (end == NULL)
    {
        return false;
    }


    size_t length =
        (size_t)(end - start);


    if (length >= size)
    {
        length =
            size - 1;
    }


    memcpy(
        gateway,
        start,
        length
    );


    gateway[length] = '\0';


    return true;
}


/*
 * ================================================================
 * Ping
 * ================================================================
 */

/*
 * Returns:
 *
 * >= 0 = RTT in milliseconds
 * -1   = timeout / failure
 */
static int ping_host(
    const char *host)
{
    char response[RESPONSE_SIZE];
    char command[128];


    snprintf(
        command,
        sizeof(command),
        "AT+PING=\"%s\"",
        host
    );


    if (!wizfi_command(
            command,
            response,
            sizeof(response),
            5000))
    {
        return -1;
    }


    /*
     * Example successful response:
     *
     * +21
     *
     * OK
     */
    char *plus =
        strchr(
            response,
            '+'
        );


    if (plus == NULL)
    {
        return -1;
    }


    /*
     * Reject something such as:
     *
     * +timeout
     */
    if (!isdigit(
            (unsigned char)plus[1]))
    {
        return -1;
    }


    return atoi(
        plus + 1
    );
}


/*
 * ================================================================
 * DNS
 * ================================================================
 */

/*
 * Resolve a hostname using the WizFi360's built-in
 * DNS resolver.
 *
 * Example command:
 *
 * AT+CIPDOMAIN="example.com"
 */
static bool dns_resolve_host(
    const char *domain,
    char *resolved_ip,
    size_t resolved_ip_size)
{
    char response[RESPONSE_SIZE];
    char command[128];


    if (
        domain == NULL ||
        resolved_ip == NULL ||
        resolved_ip_size == 0
    )
    {
        return false;
    }


    resolved_ip[0] = '\0';


    snprintf(
        command,
        sizeof(command),
        "AT+CIPDOMAIN=\"%s\"",
        domain
    );


    if (!wizfi_command(
            command,
            response,
            sizeof(response),
            5000))
    {
        return false;
    }


    const char *prefix =
        "+CIPDOMAIN:\"";


    char *start =
        strstr(
            response,
            prefix
        );


    if (start == NULL)
    {
        return false;
    }


    start += strlen(prefix);


    char *end =
        strchr(
            start,
            '"'
        );


    if (end == NULL)
    {
        return false;
    }


    size_t length =
        (size_t)(end - start);


    if (length >= resolved_ip_size)
    {
        length =
            resolved_ip_size - 1;
    }


    memcpy(
        resolved_ip,
        start,
        length
    );


    resolved_ip[length] = '\0';


    return true;
}


/*
 * ================================================================
 * Utility
 * ================================================================
 */

static uint64_t get_uptime_ms(void)
{
    return
        time_us_64()
        / 1000ULL;
}


/*
 * Work out the overall network state.
 *
 * More fundamental failures are given priority over
 * performance warnings.
 */
static const char *get_status(
    bool wifi_connected,
    int gateway_ping,
    int internet_ping,
    const network_events_t *events)
{
    if (!wifi_connected)
    {
        return "DISCONNECTED";
    }


    if (gateway_ping < 0)
    {
        return "LOCAL_NETWORK_ISSUE";
    }


    if (events->internet_outage_active)
    {
        return "INTERNET_OUTAGE";
    }


    /*
     * A probe has failed but we haven't yet reached
     * the consecutive-failure threshold required to
     * declare a confirmed outage.
     */
    if (internet_ping < 0)
    {
        return "INTERNET_ISSUE";
    }


    if (events->dns_failure_active)
    {
        return "DNS_FAILURE";
    }


    if (events->high_packet_loss_active)
    {
        return "HIGH_PACKET_LOSS";
    }


    if (events->high_latency_active)
    {
        return "HIGH_LATENCY";
    }


    if (events->high_jitter_active)
    {
        return "HIGH_JITTER";
    }


    if (events->weak_signal_active)
    {
        return "WEAK_SIGNAL";
    }


    return "ONLINE";
}


/*
 * ================================================================
 * Main
 * ================================================================
 */

int main(void)
{
    /*
     * USB serial.
     *
     * telemetry.c writes structured NDJSON here.
     */
    stdio_init_all();


    sleep_ms(3000);


    /*
     * ============================================================
     * WizFi360 UART
     * ============================================================
     */

    uart_init(
        WIZ_UART,
        115200
    );


    gpio_set_function(
        WIZ_TX_PIN,
        GPIO_FUNC_UART
    );


    gpio_set_function(
        WIZ_RX_PIN,
        GPIO_FUNC_UART
    );


    sleep_ms(1000);


    char response[RESPONSE_SIZE];


    /*
     * Device startup.
     */
    telemetry_emit_event_simple(
        get_uptime_ms(),
        "DEVICE_STARTED",
        "info"
    );


    /*
     * ============================================================
     * Verify WizFi360
     * ============================================================
     */

    if (!wizfi_command(
            "AT",
            response,
            sizeof(response),
            1000))
    {
        telemetry_emit_event_simple(
            get_uptime_ms(),
            "WIZFI360_NOT_RESPONDING",
            "critical"
        );


        while (true)
        {
            sleep_ms(1000);
        }
    }


    /*
     * Disable command echo.
     */
    wizfi_command(
        "ATE0",
        response,
        sizeof(response),
        1000
    );


    /*
     * ============================================================
     * Initial Wi-Fi connection
     * ============================================================
     */

    bool initial_connect_failure_sent =
        false;


    while (!connect_wifi(
        response,
        sizeof(response)))
    {
        /*
         * Don't generate one event every five seconds.
         */
        if (!initial_connect_failure_sent)
        {
            telemetry_emit_event_simple(
                get_uptime_ms(),
                "WIFI_CONNECT_FAILED",
                "warning"
            );


            initial_connect_failure_sent =
                true;
        }


        sleep_ms(5000);
    }


    telemetry_emit_event_simple(
        get_uptime_ms(),
        "WIFI_CONNECTED",
        "info"
    );


    /*
     * ============================================================
     * Initial gateway
     * ============================================================
     */

    char gateway[32] =
        "Unknown";


    if (!get_gateway(
            gateway,
            sizeof(gateway)))
    {
        snprintf(
            gateway,
            sizeof(gateway),
            "Unknown"
        );
    }


    /*
     * ============================================================
     * Network event subsystem
     * ============================================================
     */

    network_events_t events;


    network_events_init(
        &events
    );


    /*
     * Establish initial gateway as baseline.
     */
    if (
        strcmp(
            gateway,
            "Unknown"
        ) != 0
    )
    {
        network_events_update_gateway(
            &events,
            gateway,
            get_uptime_ms()
        );
    }


    /*
     * ============================================================
     * Statistics
     * ============================================================
     */

    uint32_t samples =
        0;


    uint32_t internet_failures =
        0;


    uint32_t gateway_failures =
        0;


    /*
     * RTT statistics.
     */
    int min_rtt =
        -1;


    int max_rtt =
        -1;


    uint64_t total_rtt =
        0;


    uint32_t successful_rtt_samples =
        0;


    /*
     * Jitter statistics.
     */
    int previous_internet_ping =
        -1;


    int current_jitter =
        -1;


    uint64_t total_jitter =
        0;


    uint32_t jitter_samples =
        0;


    /*
     * Wi-Fi outage statistics.
     */
    bool previous_wifi_connected =
        true;


    bool wifi_outage_active =
        false;


    uint32_t disconnect_count =
        0;


    uint32_t reconnect_count =
        0;


    uint32_t reconnect_attempts =
        0;


    uint64_t wifi_outage_start_ms =
        0;


    uint64_t last_reconnect_duration_ms =
        0;


    uint64_t total_wifi_outage_ms =
        0;


    /*
     * ============================================================
     * Main monitoring loop
     * ============================================================
     */

    while (true)
    {
        /*
         * Clear flags such as:
         *
         * BSSID_CHANGED
         * HIGH_JITTER
         * DNS_FAILURE
         *
         * They become true again only if a new event
         * occurs during this monitoring cycle.
         */
        network_events_begin_sample(
            &events
        );


        samples++;


        uint64_t now_ms =
            get_uptime_ms();


        /*
         * ========================================================
         * Wi-Fi state
         * ========================================================
         */

        wifi_info_t wifi =
            get_wifi_info();


        bool wifi_connected =
            wifi.valid;


        /*
         * Detect Wi-Fi disconnect edge.
         */
        if (
            !wifi_connected &&
            previous_wifi_connected
        )
        {
            disconnect_count++;


            wifi_outage_active =
                true;


            wifi_outage_start_ms =
                now_ms;


            telemetry_emit_event_simple(
                now_ms,
                "WIFI_DISCONNECTED",
                "warning"
            );
        }


        /*
         * ========================================================
         * Automatic Wi-Fi reconnect
         * ========================================================
         */

        if (!wifi_connected)
        {
            reconnect_attempts++;


            sleep_ms(
                RECONNECT_DELAY_MS
            );


            bool reconnect_result =
                connect_wifi(
                    response,
                    sizeof(response)
                );


            if (reconnect_result)
            {
                /*
                 * Verify actual association.
                 */
                wifi =
                    get_wifi_info();


                wifi_connected =
                    wifi.valid;
            }
        }


        /*
         * Wi-Fi recovered.
         */
        if (
            wifi_connected &&
            wifi_outage_active
        )
        {
            uint64_t recovered_at_ms =
                get_uptime_ms();


            last_reconnect_duration_ms =
                recovered_at_ms -
                wifi_outage_start_ms;


            total_wifi_outage_ms +=
                last_reconnect_duration_ms;


            reconnect_count++;


            wifi_outage_active =
                false;


            telemetry_emit_event_duration(
                recovered_at_ms,
                "WIFI_RECONNECTED",
                "info",
                last_reconnect_duration_ms
            );


            /*
             * DHCP information may have changed.
             */
            snprintf(
                gateway,
                sizeof(gateway),
                "Unknown"
            );


            get_gateway(
                gateway,
                sizeof(gateway)
            );
        }


        /*
         * ========================================================
         * BSSID / Channel / RSSI
         * ========================================================
         */

        if (
            wifi_connected &&
            wifi.valid
        )
        {
            /*
             * Access point changes.
             */
            network_events_update_ap(
                &events,
                wifi.bssid,
                wifi.channel,
                get_uptime_ms()
            );


            if (events.bssid_changed)
            {
                telemetry_emit_event_change_string(
                    get_uptime_ms(),
                    "BSSID_CHANGED",
                    "info",
                    events.previous_bssid,
                    events.current_bssid
                );
            }


            if (events.channel_changed)
            {
                telemetry_emit_event_change_int(
                    get_uptime_ms(),
                    "CHANNEL_CHANGED",
                    "info",
                    events.previous_channel,
                    events.current_channel
                );
            }


            /*
             * Weak signal.
             */
            network_events_update_signal(
                &events,
                wifi.rssi,
                WEAK_RSSI_THRESHOLD_DBM,
                get_uptime_ms()
            );


            if (events.weak_signal_event)
            {
                telemetry_emit_event_metric(
                    get_uptime_ms(),
                    "WEAK_SIGNAL",
                    "warning",
                    "rssi_dbm",
                    wifi.rssi,
                    WEAK_RSSI_THRESHOLD_DBM
                );
            }
        }


        /*
         * ========================================================
         * Gateway monitoring
         * ========================================================
         */

        if (
            strcmp(
                gateway,
                "Unknown"
            ) != 0
        )
        {
            network_events_update_gateway(
                &events,
                gateway,
                get_uptime_ms()
            );


            if (events.gateway_changed)
            {
                telemetry_emit_event_change_string(
                    get_uptime_ms(),
                    "GATEWAY_CHANGED",
                    "warning",
                    events.previous_gateway,
                    events.current_gateway
                );
            }
        }


        /*
         * ========================================================
         * Network probes
         * ========================================================
         */

        int gateway_ping =
            -1;


        int internet_ping =
            -1;


        if (wifi_connected)
        {
            /*
             * Local gateway RTT.
             */
            if (
                strcmp(
                    gateway,
                    "Unknown"
                ) != 0
            )
            {
                gateway_ping =
                    ping_host(
                        gateway
                    );
            }


            /*
             * Internet RTT.
             */
            internet_ping =
                ping_host(
                    INTERNET_TARGET
                );
        }


        /*
         * ========================================================
         * Internet outage classification
         * ========================================================
         *
         * Only classify the problem as an Internet outage
         * when:
         *
         * Wi-Fi is connected
         * AND
         * gateway responds
         *
         * Otherwise the root cause is lower in the stack.
         */

        bool internet_test_eligible =
            wifi_connected &&
            gateway_ping >= 0;


        network_events_update_internet(
            &events,
            internet_test_eligible,
            internet_ping >= 0,
            INTERNET_FAILURE_THRESHOLD,
            get_uptime_ms()
        );


        if (events.internet_outage_event)
        {
            telemetry_emit_event_simple(
                get_uptime_ms(),
                "INTERNET_OUTAGE",
                "critical"
            );
        }


        if (events.internet_recovered_event)
        {
            telemetry_emit_event_duration(
                get_uptime_ms(),
                "INTERNET_RECOVERED",
                "info",
                events.last_internet_outage_duration_ms
            );
        }


        /*
         * ========================================================
         * DNS monitoring
         * ========================================================
         *
         * DNS is tested only if raw IP Internet access
         * already works.
         *
         * This distinguishes:
         *
         * INTERNET_OUTAGE
         *
         * from:
         *
         * DNS_FAILURE
         */

        bool dns_test_performed =
            false;


        bool dns_success =
            false;


        char resolved_ip[64] =
            "";


        if (
            wifi_connected &&
            gateway_ping >= 0 &&
            internet_ping >= 0
        )
        {
            dns_test_performed =
                true;


            dns_success =
                dns_resolve_host(
                    DNS_TEST_DOMAIN,
                    resolved_ip,
                    sizeof(resolved_ip)
                );


            network_events_update_dns(
                &events,
                dns_success,
                DNS_FAILURE_THRESHOLD,
                get_uptime_ms()
            );
        }


        if (
            dns_test_performed &&
            events.dns_failure_event
        )
        {
            telemetry_emit_event_simple(
                get_uptime_ms(),
                "DNS_FAILURE",
                "warning"
            );
        }


        if (
            dns_test_performed &&
            events.dns_recovered_event
        )
        {
            telemetry_emit_event_simple(
                get_uptime_ms(),
                "DNS_RECOVERED",
                "info"
            );
        }


        /*
         * ========================================================
         * High latency
         * ========================================================
         */

        network_events_update_latency(
            &events,
            internet_ping,
            HIGH_RTT_THRESHOLD_MS,
            get_uptime_ms()
        );


        if (events.high_latency_event)
        {
            telemetry_emit_event_metric(
                get_uptime_ms(),
                "HIGH_LATENCY",
                "warning",
                "internet_rtt_ms",
                internet_ping,
                HIGH_RTT_THRESHOLD_MS
            );
        }


        /*
         * ========================================================
         * Failure statistics
         * ========================================================
         */

        if (gateway_ping < 0)
        {
            gateway_failures++;
        }


        if (internet_ping < 0)
        {
            internet_failures++;
        }


        /*
         * ========================================================
         * RTT statistics
         * ========================================================
         */

        if (internet_ping >= 0)
        {
            if (
                min_rtt < 0 ||
                internet_ping < min_rtt
            )
            {
                min_rtt =
                    internet_ping;
            }


            if (
                max_rtt < 0 ||
                internet_ping > max_rtt
            )
            {
                max_rtt =
                    internet_ping;
            }


            total_rtt +=
                (uint64_t)
                internet_ping;


            successful_rtt_samples++;
        }


        float average_rtt =
            0.0f;


        if (
            successful_rtt_samples > 0
        )
        {
            average_rtt =
                (float)total_rtt
                /
                (float)
                successful_rtt_samples;
        }


        /*
         * ========================================================
         * Jitter
         * ========================================================
         *
         * Jitter is currently calculated as the absolute
         * difference between consecutive successful RTT
         * measurements.
         */

        if (internet_ping >= 0)
        {
            if (
                previous_internet_ping >= 0
            )
            {
                current_jitter =
                    abs(
                        internet_ping -
                        previous_internet_ping
                    );


                total_jitter +=
                    (uint64_t)
                    current_jitter;


                jitter_samples++;
            }
            else
            {
                current_jitter =
                    -1;
            }


            previous_internet_ping =
                internet_ping;
        }
        else
        {
            /*
             * Break consecutive RTT chain.
             */
            current_jitter =
                -1;


            previous_internet_ping =
                -1;
        }


        float average_jitter =
            0.0f;


        if (jitter_samples > 0)
        {
            average_jitter =
                (float)total_jitter
                /
                (float)jitter_samples;
        }


        /*
         * ========================================================
         * High jitter event
         * ========================================================
         */

        network_events_update_jitter(
            &events,
            current_jitter,
            HIGH_JITTER_THRESHOLD_MS,
            get_uptime_ms()
        );


        if (events.high_jitter_event)
        {
            telemetry_emit_event_metric(
                get_uptime_ms(),
                "HIGH_JITTER",
                "warning",
                "jitter_ms",
                current_jitter,
                HIGH_JITTER_THRESHOLD_MS
            );
        }


        /*
         * ========================================================
         * Rolling packet-loss monitoring
         * ========================================================
         *
         * Wi-Fi outages are excluded from this rolling
         * Internet-loss window.
         *
         * The ordinary lifetime packet-loss statistic below
         * still includes all failed Internet probes.
         */

        if (wifi_connected)
        {
            network_events_update_packet_loss(
                &events,
                internet_ping >= 0,
                HIGH_PACKET_LOSS_THRESHOLD_PCT,
                PACKET_LOSS_MIN_SAMPLES,
                get_uptime_ms()
            );
        }


        if (events.high_packet_loss_event)
        {
            telemetry_emit_event_metric(
                get_uptime_ms(),
                "HIGH_PACKET_LOSS",
                "warning",
                "packet_loss_pct",
                (int)
                    events.current_packet_loss_window_pct,
                HIGH_PACKET_LOSS_THRESHOLD_PCT
            );
        }


        /*
         * ========================================================
         * Lifetime packet loss
         * ========================================================
         */

        float internet_loss =
            (
                (float)
                internet_failures
                /
                (float)samples
            )
            * 100.0f;


        float gateway_loss =
            (
                (float)
                gateway_failures
                /
                (float)samples
            )
            * 100.0f;


        /*
         * ========================================================
         * Overall status
         * ========================================================
         */

        const char *status =
            get_status(
                wifi_connected,
                gateway_ping,
                internet_ping,
                &events
            );


        /*
         * ========================================================
         * Structured measurement
         * ========================================================
         */

        telemetry_measurement_t measurement =
        {
            .uptime_ms =
                get_uptime_ms(),


            .status =
                status,


            .ssid =
                wifi.valid
                    ? wifi.ssid
                    : "",


            .bssid =
                wifi.valid
                    ? wifi.bssid
                    : "",


            .channel =
                wifi.valid
                    ? wifi.channel
                    : -1,


            .rssi_dbm =
                wifi.valid
                    ? wifi.rssi
                    : -1,


            .gateway =
                gateway,


            .gateway_rtt_ms =
                gateway_ping,


            .gateway_loss_pct =
                gateway_loss,


            .internet_rtt_ms =
                internet_ping,


            .min_rtt_ms =
                min_rtt,


            .avg_rtt_ms =
                average_rtt,


            .max_rtt_ms =
                max_rtt,


            .jitter_ms =
                current_jitter,


            .avg_jitter_ms =
                average_jitter,


            .packet_loss_pct =
                internet_loss,


            .samples =
                samples,


            .successful =
                successful_rtt_samples,


            .failed =
                internet_failures,


            .disconnects =
                disconnect_count,


            .reconnects =
                reconnect_count,


            .reconnect_attempts =
                reconnect_attempts,


            .bssid_changes =
                events.bssid_change_count,


            .channel_changes =
                events.channel_change_count,


            .gateway_changes =
                events.gateway_change_count,


            .weak_signal_events =
                events.weak_signal_event_count,


            .high_latency_events =
                events.high_latency_event_count,


            .weak_signal_active =
                events.weak_signal_active,


            .high_latency_active =
                events.high_latency_active
        };


        telemetry_emit_measurement(
            &measurement
        );


        /*
         * ========================================================
         * Save state for next monitoring cycle
         * ========================================================
         */

        previous_wifi_connected =
            wifi_connected;


        /*
         * All historical storage happens on the laptop.
         *
         * The Pico only keeps small counters and event state
         * in RAM.
         */
        sleep_ms(
            MONITOR_INTERVAL_MS
        );
    }
}
