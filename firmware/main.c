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
 * Send an AT command to the WizFi360.
 *
 * The response is captured in the provided buffer.
 * Nothing is printed directly to USB.
 */
static bool wizfi_command(
    const char *command,
    char *response,
    size_t response_size,
    uint32_t timeout_ms)
{
    /*
     * Clear stale UART data.
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
 * Connect to the configured Wi-Fi network.
 */
static bool connect_wifi(
    char *response,
    size_t response_size)
{
    /*
     * Put WizFi360 into Station mode.
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
 * Read information about the currently
 * associated Wi-Fi access point.
 *
 * Expected WizFi360 response:
 *
 * +CWJAP_CUR:"SSID","BSSID",channel,RSSI
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
 * Get the default gateway assigned to the
 * WizFi360.
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
 * Ping a host.
 *
 * Returns:
 *
 * >= 0    RTT in milliseconds
 * -1      timeout / failure
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
     * Successful response contains something like:
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
     * Reject:
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
 * Return RP2040 uptime in milliseconds.
 */
static uint64_t get_uptime_ms(void)
{
    return
        time_us_64()
        / 1000ULL;
}


/*
 * Work out the current overall diagnostic state.
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


    if (internet_ping < 0)
    {
        return "INTERNET_ISSUE";
    }


    if (events->high_latency_active)
    {
        return "HIGH_LATENCY";
    }


    if (events->weak_signal_active)
    {
        return "WEAK_SIGNAL";
    }


    return "ONLINE";
}


int main(void)
{
    /*
     * USB serial output.
     *
     * All telemetry will be emitted over this
     * connection as NDJSON.
     */
    stdio_init_all();


    /*
     * Give USB serial time to initialize.
     */
    sleep_ms(3000);


    /*
     * Configure RP2040 -> WizFi360 UART.
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
     * Emit startup event.
     */
    telemetry_emit_event_simple(
        get_uptime_ms(),
        "DEVICE_STARTED",
        "info"
    );


    /*
     * Verify communication with WizFi360.
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
     * Disable AT command echo.
     */
    wizfi_command(
        "ATE0",
        response,
        sizeof(response),
        1000
    );


    /*
     * Initial Wi-Fi connection.
     *
     * Keep retrying instead of stopping the device.
     */
    bool initial_connect_event_sent =
        false;


    while (!connect_wifi(
        response,
        sizeof(response)))
    {
        if (!initial_connect_event_sent)
        {
            telemetry_emit_event_simple(
                get_uptime_ms(),
                "WIFI_CONNECT_FAILED",
                "warning"
            );


            initial_connect_event_sent =
                true;
        }


        sleep_ms(5000);
    }


    /*
     * Wi-Fi is connected.
     */
    telemetry_emit_event_simple(
        get_uptime_ms(),
        "WIFI_CONNECTED",
        "info"
    );


    /*
     * Obtain initial gateway.
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
     * Initialize network-event subsystem.
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
     * General probe statistics.
     */
    uint32_t samples = 0;

    uint32_t internet_failures = 0;

    uint32_t gateway_failures = 0;


    /*
     * RTT statistics.
     */
    int min_rtt = -1;

    int max_rtt = -1;

    uint64_t total_rtt = 0;

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
     * Disconnect/reconnect statistics.
     */
    bool previous_wifi_connected =
        true;

    bool outage_active =
        false;


    uint32_t disconnect_count =
        0;

    uint32_t reconnect_count =
        0;

    uint32_t reconnect_attempts =
        0;


    uint64_t outage_start_ms =
        0;

    uint64_t last_reconnect_duration_ms =
        0;

    uint64_t total_outage_ms =
        0;


    /*
     * Main monitoring loop.
     */
    while (true)
    {
        /*
         * Reset one-sample event flags.
         */
        network_events_begin_sample(
            &events
        );


        samples++;


        uint64_t now_ms =
            get_uptime_ms();


        /*
         * Read current Wi-Fi association.
         */
        wifi_info_t wifi =
            get_wifi_info();


        bool wifi_connected =
            wifi.valid;


        /*
         * Detect a new Wi-Fi outage.
         */
        if (
            !wifi_connected &&
            previous_wifi_connected
        )
        {
            disconnect_count++;


            outage_active =
                true;


            outage_start_ms =
                now_ms;


            telemetry_emit_event_simple(
                now_ms,
                "WIFI_DISCONNECTED",
                "warning"
            );
        }


        /*
         * Attempt automatic recovery.
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


            /*
             * Verify that the module really
             * re-associated.
             */
            if (reconnect_result)
            {
                wifi =
                    get_wifi_info();


                wifi_connected =
                    wifi.valid;
            }
        }


        /*
         * Successful recovery from an outage.
         */
        if (
            wifi_connected &&
            outage_active
        )
        {
            uint64_t recovered_at_ms =
                get_uptime_ms();


            last_reconnect_duration_ms =
                recovered_at_ms -
                outage_start_ms;


            total_outage_ms +=
                last_reconnect_duration_ms;


            reconnect_count++;


            outage_active =
                false;


            /*
             * Report recovery event.
             */
            telemetry_emit_event_duration(
                recovered_at_ms,
                "WIFI_RECONNECTED",
                "info",
                last_reconnect_duration_ms
            );


            /*
             * DHCP information may have changed
             * during the reconnect.
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
         * Access point + signal event detection.
         */
        if (
            wifi_connected &&
            wifi.valid
        )
        {
            /*
             * BSSID + channel.
             */
            network_events_update_ap(
                &events,
                wifi.bssid,
                wifi.channel,
                get_uptime_ms()
            );


            /*
             * BSSID event.
             */
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


            /*
             * Channel event.
             */
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
             * Signal threshold monitoring.
             */
            network_events_update_signal(
                &events,
                wifi.rssi,
                WEAK_RSSI_THRESHOLD_DBM,
                get_uptime_ms()
            );


            /*
             * Emit event only when entering
             * weak-signal state.
             */
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
         * Gateway monitoring.
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
         * Run network probes.
         */
        int gateway_ping =
            -1;


        int internet_ping =
            -1;


        if (wifi_connected)
        {
            /*
             * Local-network / gateway RTT.
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
             * External Internet RTT.
             */
            internet_ping =
                ping_host(
                    INTERNET_TARGET
                );
        }


        /*
         * High-latency monitoring.
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
         * Count failures.
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
         * Update RTT history.
         *
         * Only successful Internet pings
         * contribute to these statistics.
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
                (uint64_t)internet_ping;


            successful_rtt_samples++;
        }


        /*
         * Average RTT.
         */
        float average_rtt =
            0.0f;


        if (
            successful_rtt_samples > 0
        )
        {
            average_rtt =
                (float)total_rtt /
                (float)
                successful_rtt_samples;
        }


        /*
         * Jitter.
         *
         * Current implementation:
         *
         * absolute difference between consecutive
         * successful Internet RTT measurements.
         */
        if (internet_ping >= 0)
        {
            if (
                previous_internet_ping
                >= 0
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
             * Break the jitter sequence when
             * connectivity is lost.
             */
            current_jitter =
                -1;


            previous_internet_ping =
                -1;
        }


        /*
         * Average jitter.
         */
        float average_jitter =
            0.0f;


        if (jitter_samples > 0)
        {
            average_jitter =
                (float)total_jitter /
                (float)jitter_samples;
        }


        /*
         * Packet-loss percentages.
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
         * Current overall device status.
         */
        const char *status =
            get_status(
                wifi_connected,
                gateway_ping,
                internet_ping,
                &events
            );


        /*
         * Emit structured measurement.
         *
         * This is one complete JSON line.
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
         * Store state for next monitoring cycle.
         */
        previous_wifi_connected =
            wifi_connected;


        /*
         * The Pico itself stores no historical
         * measurement data.
         *
         * The laptop collector receives each
         * record and stores it in SQLite.
         */
        sleep_ms(
            MONITOR_INTERVAL_MS
        );
    }
}
