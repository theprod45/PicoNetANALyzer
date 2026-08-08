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


/*
 * ================================================================
 * Data structures
 * ================================================================
 */

typedef struct
{
    char ssid[64];
    char bssid[32];

    int channel;
    int rssi;

    bool valid;

} wifi_info_t;


typedef struct
{
    char ip_address[32];
    char gateway[32];
    char netmask[32];

    bool valid;

} network_info_t;


/*
 * ================================================================
 * WizFi360 AT interface
 * ================================================================
 */

static bool wizfi_command(
    const char *command,
    char *response,
    size_t response_size,
    uint32_t timeout_ms)
{
    /*
     * Remove stale UART bytes before sending a new command.
     */
    while (uart_is_readable(WIZ_UART))
    {
        uart_getc(WIZ_UART);
    }


    response[0] =
        '\0';


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


    size_t pos =
        0;


    while (!time_reached(timeout))
    {
        while (uart_is_readable(WIZ_UART))
        {
            char c =
                uart_getc(WIZ_UART);


            if (
                pos <
                response_size - 1
            )
            {
                response[pos++] =
                    c;


                response[pos] =
                    '\0';
            }


            /*
             * Successful AT command.
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
             * Failed AT command.
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
 * Read current SSID/BSSID/channel/RSSI.
 */
static wifi_info_t get_wifi_info(void)
{
    wifi_info_t info =
        {0};


    char response[
        RESPONSE_SIZE
    ];


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
        info.valid =
            true;
    }


    return info;
}


/*
 * ================================================================
 * Generic response parsing
 * ================================================================
 */

static bool extract_quoted_value(
    const char *response,
    const char *prefix,
    char *output,
    size_t output_size)
{
    if (
        response == NULL ||
        prefix == NULL ||
        output == NULL ||
        output_size == 0
    )
    {
        return false;
    }


    char *start =
        strstr(
            response,
            prefix
        );


    if (start == NULL)
    {
        return false;
    }


    start +=
        strlen(prefix);


    /*
     * Skip whitespace.
     */
    while (
        *start == ' ' ||
        *start == '\t'
    )
    {
        start++;
    }


    bool quoted =
        *start == '"';


    if (quoted)
    {
        start++;
    }


    char *end;


    if (quoted)
    {
        end =
            strchr(
                start,
                '"'
            );
    }
    else
    {
        end =
            strpbrk(
                start,
                "\r\n"
            );
    }


    if (end == NULL)
    {
        return false;
    }


    size_t length =
        (size_t)(
            end - start
        );


    if (
        length >=
        output_size
    )
    {
        length =
            output_size - 1;
    }


    memcpy(
        output,
        start,
        length
    );


    output[length] =
        '\0';


    return true;
}


/*
 * ================================================================
 * Current station IP information
 * ================================================================
 */

static network_info_t get_network_info(void)
{
    network_info_t info =
        {0};


    char response[
        RESPONSE_SIZE
    ];


    if (!wizfi_command(
            "AT+CIPSTA_CUR?",
            response,
            sizeof(response),
            2000))
    {
        return info;
    }


    bool ip_ok =
        extract_quoted_value(
            response,
            "+CIPSTA_CUR:ip:",
            info.ip_address,
            sizeof(info.ip_address)
        );


    bool gateway_ok =
        extract_quoted_value(
            response,
            "+CIPSTA_CUR:gateway:",
            info.gateway,
            sizeof(info.gateway)
        );


    extract_quoted_value(
        response,
        "+CIPSTA_CUR:netmask:",
        info.netmask,
        sizeof(info.netmask)
    );


    info.valid =
        ip_ok &&
        gateway_ok;


    return info;
}


/*
 * ================================================================
 * DNS server information
 * ================================================================
 */

static bool get_primary_dns_server(
    char *dns_server,
    size_t dns_server_size)
{
    if (
        dns_server == NULL ||
        dns_server_size == 0
    )
    {
        return false;
    }


    dns_server[0] =
        '\0';


    char response[
        RESPONSE_SIZE
    ];


    if (!wizfi_command(
            "AT+CIPDNS_CUR?",
            response,
            sizeof(response),
            2000))
    {
        return false;
    }


    const char *prefix =
        "+CIPDNS_CUR:";


    char *start =
        strstr(
            response,
            prefix
        );


    if (start == NULL)
    {
        return false;
    }


    start +=
        strlen(prefix);


    /*
     * The documented response may contain a space:
     *
     * +CIPDNS_CUR: 1.1.1.1
     */
    while (
        *start == ' ' ||
        *start == '\t'
    )
    {
        start++;
    }


    /*
     * Also tolerate quoted firmware responses.
     */
    if (*start == '"')
    {
        start++;
    }


    char *end =
        start;


    while (
        *end != '\0' &&
        *end != '"' &&
        *end != '\r' &&
        *end != '\n'
    )
    {
        end++;
    }


    if (end == start)
    {
        return false;
    }


    size_t length =
        (size_t)(
            end - start
        );


    if (
        length >=
        dns_server_size
    )
    {
        length =
            dns_server_size - 1;
    }


    memcpy(
        dns_server,
        start,
        length
    );


    dns_server[length] =
        '\0';


    return true;
}


/*
 * ================================================================
 * Ping
 * ================================================================
 */

/*
 * Return:
 *
 * >= 0 = RTT in milliseconds
 * -1   = failed / timeout
 */
static int ping_host(
    const char *host)
{
    char response[
        RESPONSE_SIZE
    ];


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
     * Typical response contains:
     *
     * +21
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


    if (!isdigit(
            (unsigned char)
            plus[1]))
    {
        return -1;
    }


    return atoi(
        plus + 1
    );
}


/*
 * ================================================================
 * DNS resolution and DNS latency
 * ================================================================
 */

/*
 * DNS latency here measures the time taken for the
 * WizFi360 AT+CIPDOMAIN request to complete.
 *
 * This is not an ICMP ping to the DNS resolver.
 */
static bool dns_resolve_host(
    const char *domain,
    char *resolved_ip,
    size_t resolved_ip_size,
    int *latency_ms)
{
    if (
        domain == NULL ||
        resolved_ip == NULL ||
        resolved_ip_size == 0 ||
        latency_ms == NULL
    )
    {
        return false;
    }


    resolved_ip[0] =
        '\0';


    *latency_ms =
        -1;


    char response[
        RESPONSE_SIZE
    ];


    char command[128];


    snprintf(
        command,
        sizeof(command),
        "AT+CIPDOMAIN=\"%s\"",
        domain
    );


    uint64_t start_us =
        time_us_64();


    bool success =
        wizfi_command(
            command,
            response,
            sizeof(response),
            5000
        );


    uint64_t end_us =
        time_us_64();


    if (!success)
    {
        return false;
    }


    /*
     * Verify a valid DNS result exists.
     */
    const char *prefix =
        "+CIPDOMAIN:";


    char *start =
        strstr(
            response,
            prefix
        );


    if (start == NULL)
    {
        return false;
    }


    start +=
        strlen(prefix);


    while (
        *start == ' ' ||
        *start == '\t'
    )
    {
        start++;
    }


    if (*start == '"')
    {
        start++;
    }


    char *end =
        start;


    while (
        *end != '\0' &&
        *end != '"' &&
        *end != '\r' &&
        *end != '\n'
    )
    {
        end++;
    }


    if (end == start)
    {
        return false;
    }


    size_t length =
        (size_t)(
            end - start
        );


    if (
        length >=
        resolved_ip_size
    )
    {
        length =
            resolved_ip_size - 1;
    }


    memcpy(
        resolved_ip,
        start,
        length
    );


    resolved_ip[length] =
        '\0';


    /*
     * Only record latency after confirming that DNS
     * actually returned an address.
     */
    *latency_ms =
        (int)(
            (end_us - start_us)
            /
            1000ULL
        );


    return true;
}


/*
 * ================================================================
 * General helpers
 * ================================================================
 */

static uint64_t get_uptime_ms(void)
{
    return
        time_us_64()
        /
        1000ULL;
}


static const char *get_status(
    bool wifi_connected,
    int gateway_ping,
    int internet_ping,
    const network_events_t *events)
{
    /*
     * Fundamental connectivity failures take priority
     * over performance warnings.
     */

    if (!wifi_connected)
    {
        return "DISCONNECTED";
    }


    if (gateway_ping < 0)
    {
        return "LOCAL_NETWORK_ISSUE";
    }


    if (
        events->
        internet_outage_active
    )
    {
        return "INTERNET_OUTAGE";
    }


    if (internet_ping < 0)
    {
        return "INTERNET_ISSUE";
    }


    if (
        events->
        dns_failure_active
    )
    {
        return "DNS_FAILURE";
    }


    if (
        events->
        high_packet_loss_active
    )
    {
        return "HIGH_PACKET_LOSS";
    }


    if (
        events->
        high_latency_active
    )
    {
        return "HIGH_LATENCY";
    }


    if (
        events->
        high_jitter_active
    )
    {
        return "HIGH_JITTER";
    }


    if (
        events->
        weak_signal_active
    )
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
     * USB telemetry.
     */
    stdio_init_all();


    sleep_ms(3000);


    /*
     * WizFi360 UART.
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


    char response[
        RESPONSE_SIZE
    ];


    telemetry_emit_event_simple(
        get_uptime_ms(),
        "DEVICE_STARTED",
        "info"
    );


    /*
     * Verify WizFi360.
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
        if (
            !initial_connect_failure_sent
        )
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
     * Event subsystem
     * ============================================================
     */

    network_events_t events;


    network_events_init(
        &events
    );


    /*
     * ============================================================
     * Current network identity
     * ============================================================
     */

    char gateway[32] =
        "Unknown";


    char current_ip[32] =
        "Unknown";


    char previous_ip[32] =
        "";


    bool ip_initialized =
        false;


    char dns_server[64] =
        "Unknown";


    /*
     * Establish initial IP/gateway baseline.
     */
    network_info_t initial_network =
        get_network_info();


    if (initial_network.valid)
    {
        snprintf(
            current_ip,
            sizeof(current_ip),
            "%s",
            initial_network.ip_address
        );


        snprintf(
            previous_ip,
            sizeof(previous_ip),
            "%s",
            initial_network.ip_address
        );


        ip_initialized =
            true;


        snprintf(
            gateway,
            sizeof(gateway),
            "%s",
            initial_network.gateway
        );


        network_events_update_gateway(
            &events,
            gateway,
            get_uptime_ms()
        );
    }


    get_primary_dns_server(
        dns_server,
        sizeof(dns_server)
    );


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
     * Jitter.
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
     * Wi-Fi outage tracking.
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
     * Silence compiler warnings for state retained for
     * future telemetry expansion.
     */
    (void)total_wifi_outage_ms;


    /*
     * ============================================================
     * Main monitoring loop
     * ============================================================
     */

    while (true)
    {
        network_events_begin_sample(
            &events
        );


        samples++;


        uint64_t now_ms =
            get_uptime_ms();


        /*
         * ========================================================
         * Wi-Fi information
         * ========================================================
         */

        wifi_info_t wifi =
            get_wifi_info();


        bool wifi_connected =
            wifi.valid;


        /*
         * Disconnect edge.
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
         * Automatic reconnect.
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
                wifi =
                    get_wifi_info();


                wifi_connected =
                    wifi.valid;
            }
        }


        /*
         * Wi-Fi recovery.
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
        }


        /*
         * ========================================================
         * Wi-Fi events
         * ========================================================
         */

        if (
            wifi_connected &&
            wifi.valid
        )
        {
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


            if (
                events.channel_changed
            )
            {
                telemetry_emit_event_change_int(
                    get_uptime_ms(),
                    "CHANNEL_CHANGED",
                    "info",
                    events.previous_channel,
                    events.current_channel
                );
            }


            network_events_update_signal(
                &events,
                wifi.rssi,
                WEAK_RSSI_THRESHOLD_DBM,
                get_uptime_ms()
            );


            if (
                events.weak_signal_event
            )
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
         * IP / Gateway information
         * ========================================================
         */

        if (wifi_connected)
        {
            network_info_t network_info =
                get_network_info();


            if (network_info.valid)
            {
                /*
                 * IP baseline or IP change.
                 */
                if (!ip_initialized)
                {
                    snprintf(
                        current_ip,
                        sizeof(current_ip),
                        "%s",
                        network_info.ip_address
                    );


                    snprintf(
                        previous_ip,
                        sizeof(previous_ip),
                        "%s",
                        network_info.ip_address
                    );


                    ip_initialized =
                        true;
                }
                else if (
                    strcmp(
                        current_ip,
                        network_info.ip_address
                    ) != 0
                )
                {
                    snprintf(
                        previous_ip,
                        sizeof(previous_ip),
                        "%s",
                        current_ip
                    );


                    snprintf(
                        current_ip,
                        sizeof(current_ip),
                        "%s",
                        network_info.ip_address
                    );


                    telemetry_emit_event_change_string(
                        get_uptime_ms(),
                        "IP_ADDRESS_CHANGED",
                        "info",
                        previous_ip,
                        current_ip
                    );
                }


                /*
                 * Update gateway every cycle.
                 *
                 * This also allows real gateway changes to
                 * be detected without requiring a reconnect.
                 */
                snprintf(
                    gateway,
                    sizeof(gateway),
                    "%s",
                    network_info.gateway
                );


                network_events_update_gateway(
                    &events,
                    gateway,
                    get_uptime_ms()
                );


                if (
                    events.gateway_changed
                )
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
             * Refresh primary DNS resolver.
             */
            char discovered_dns[64];


            if (
                get_primary_dns_server(
                    discovered_dns,
                    sizeof(discovered_dns)
                )
            )
            {
                snprintf(
                    dns_server,
                    sizeof(dns_server),
                    "%s",
                    discovered_dns
                );
            }
        }


        /*
         * ========================================================
         * Gateway + Internet probes
         * ========================================================
         */

        int gateway_ping =
            -1;


        int internet_ping =
            -1;


        if (wifi_connected)
        {
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


            internet_ping =
                ping_host(
                    INTERNET_TARGET
                );
        }


        /*
         * ========================================================
         * Internet outage detection
         * ========================================================
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


        if (
            events.internet_outage_event
        )
        {
            telemetry_emit_event_simple(
                get_uptime_ms(),
                "INTERNET_OUTAGE",
                "critical"
            );
        }


        if (
            events.internet_recovered_event
        )
        {
            telemetry_emit_event_duration(
                get_uptime_ms(),
                "INTERNET_RECOVERED",
                "info",
                events.
                    last_internet_outage_duration_ms
            );
        }


        /*
         * ========================================================
         * DNS test + DNS latency
         * ========================================================
         */

        bool dns_test_performed =
            false;


        bool dns_success =
            false;


        int dns_latency_ms =
            -1;


        char resolved_ip[64] =
            "";


        /*
         * Only test DNS when ordinary IP connectivity
         * is already confirmed.
         */
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
                    sizeof(resolved_ip),
                    &dns_latency_ms
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


        if (
            events.high_latency_event
        )
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
         * Failure counters
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
                (float)
                jitter_samples;
        }


        /*
         * High jitter.
         */
        network_events_update_jitter(
            &events,
            current_jitter,
            HIGH_JITTER_THRESHOLD_MS,
            get_uptime_ms()
        );


        if (
            events.high_jitter_event
        )
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
         * Rolling packet loss
         * ========================================================
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


        if (
            events.high_packet_loss_event
        )
        {
            telemetry_emit_event_metric(
                get_uptime_ms(),
                "HIGH_PACKET_LOSS",
                "warning",
                "packet_loss_pct",
                (int)
                    events.
                    current_packet_loss_window_pct,
                HIGH_PACKET_LOSS_THRESHOLD_PCT
            );
        }


        /*
         * ========================================================
         * Lifetime loss
         * ========================================================
         */

        float internet_loss =
            (
                (float)
                internet_failures
                /
                (float)samples
            )
            *
            100.0f;


        float gateway_loss =
            (
                (float)
                gateway_failures
                /
                (float)samples
            )
            *
            100.0f;


        /*
         * ========================================================
         * Overall state
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
         * Measurement telemetry
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


            .ip_address =
                ip_initialized
                    ? current_ip
                    : "Unknown",


            .gateway =
                gateway,


            .gateway_rtt_ms =
                gateway_ping,


            .gateway_loss_pct =
                gateway_loss,


            .dns_server =
                dns_server,


            .dns_test_domain =
                DNS_TEST_DOMAIN,


            .dns_latency_ms =
                dns_latency_ms,


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
         * Next-cycle state.
         */
        previous_wifi_connected =
            wifi_connected;


        sleep_ms(
            MONITOR_INTERVAL_MS
        );
    }
}
