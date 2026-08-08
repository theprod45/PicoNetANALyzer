const POLL_INTERVAL_MS = 2000;
const HISTORY_LIMIT = 180;
const EVENT_LIMIT = 100;


let selectedDevice = null;


/*
 * ================================================================
 * DOM helpers
 * ================================================================
 */

function element(id) {
    return document.getElementById(id);
}


function setText(id, value) {
    const target = element(id);

    if (target) {
        target.textContent = value;
    }
}


function formatMs(value) {
    if (
        value === null ||
        value === undefined
    ) {
        return "--";
    }

    return `${value} ms`;
}


function formatPercent(value) {
    if (
        value === null ||
        value === undefined
    ) {
        return "--";
    }

    return `${Number(value).toFixed(2)}%`;
}


function formatTime(value) {
    if (!value) {
        return "--";
    }

    const date = new Date(value);

    if (
        Number.isNaN(
            date.getTime()
        )
    ) {
        return value;
    }

    return date.toLocaleTimeString();
}


function apiUrl(path, parameters = {}) {
    const url = new URL(
        path,
        window.location.origin
    );

    if (selectedDevice) {
        url.searchParams.set(
            "device_id",
            selectedDevice
        );
    }

    for (
        const [key, value]
        of Object.entries(parameters)
    ) {
        if (
            value !== null &&
            value !== undefined &&
            value !== ""
        ) {
            url.searchParams.set(
                key,
                value
            );
        }
    }

    return url.toString();
}


async function getJson(url) {
    const response = await fetch(
        url,
        {
            cache: "no-store"
        }
    );

    if (!response.ok) {
        throw new Error(
            `${response.status} ${response.statusText}`
        );
    }

    return response.json();
}


/*
 * ================================================================
 * Chart helpers
 * ================================================================
 */

function makeLineChart(
    canvasId,
    label,
    yAxisText,
    color,
) {
    return new Chart(
        element(canvasId),
        {
            type: "line",

            data: {
                labels: [],

                datasets: [
                    {
                        label: label,
                        data: [],
                        
						borderColor: color,
						backgroundColor: color,
						pointBackgroundColor: color,
						
                        tension: 0.2,
                        pointRadius: 1,
                        borderWidth: 2,

                        spanGaps: false,
                    }
                ]
            },

            options: {
                responsive: true,
                maintainAspectRatio: false,

                animation: false,

                interaction: {
                    intersect: false,
                    mode: "index",
                },

                plugins: {
                    legend: {
                        display: false,
                    }
                },

                scales: {
                    x: {
                        ticks: {
                            maxTicksLimit: 8,
                        }
                    },

                    y: {
                        title: {
                            display: true,
                            text: yAxisText,
                        }
                    }
                }
            }
        }
    );
}


const rttChart = makeLineChart(
    "rttChart",
    "Internet RTT",
    "ms",
    "#60a5fa"
);


const dnsLatencyChart = makeLineChart(
    "dnsLatencyChart",
    "DNS Latency",
    "ms",
    "#a78bfa"
);



const rssiChart = makeLineChart(
    "rssiChart",
    "RSSI",
    "dBm",
    "#36d399"
);


const jitterChart = makeLineChart(
    "jitterChart",
    "Jitter",
    "ms",
    "#f7c948"
);

const lossChart = makeLineChart(
    "lossChart",
    "Packet Loss",
    "%",
    "#ff6b6b"
);

/*
 * ================================================================
 * Devices
 * ================================================================
 */

async function loadDevices() {
    const data = await getJson(
        "/api/devices"
    );

    const devices = (
        data.devices || []
    );


    const selector = element(
        "deviceSelector"
    );


    const previousValue =
        selectedDevice;


    selector.innerHTML = "";


    if (devices.length === 0) {
        const option =
            document.createElement(
                "option"
            );

        option.value = "";
        option.textContent =
            "No devices";

        selector.appendChild(
            option
        );

        selectedDevice = null;

        return;
    }


    for (const device of devices) {
        const option =
            document.createElement(
                "option"
            );

        option.value =
            device.device_id;

        option.textContent =
            device.device_id;

        selector.appendChild(
            option
        );
    }


    if (
        previousValue &&
        devices.some(
            item =>
                item.device_id ===
                previousValue
        )
    ) {
        selectedDevice =
            previousValue;
    }
    else {
        selectedDevice =
            devices[0].device_id;
    }


    selector.value =
        selectedDevice;


    updateExportLinks();
}


function updateExportLinks() {
    const measurements =
        element(
            "exportMeasurements"
        );

    const events =
        element(
            "exportEvents"
        );


    measurements.href =
        apiUrl(
            "/api/export/measurements.csv"
        );


    events.href =
        apiUrl(
            "/api/export/events.csv"
        );
}


/*
 * ================================================================
 * Status card
 * ================================================================
 */

function statusClass(status) {
    const critical = new Set([
        "DISCONNECTED",
        "INTERNET_OUTAGE",
    ]);


    const warning = new Set([
        "LOCAL_NETWORK_ISSUE",
        "INTERNET_ISSUE",
        "DNS_FAILURE",
        "HIGH_PACKET_LOSS",
        "HIGH_LATENCY",
        "HIGH_JITTER",
        "WEAK_SIGNAL",
    ]);


    if (critical.has(status)) {
        return "status-critical";
    }


    if (warning.has(status)) {
        return "status-warning";
    }


    return "status-online";
}


async function updateStatus() {
    if (!selectedDevice) {
        return;
    }


    const latest = await getJson(
        apiUrl(
            "/api/status"
        )
    );


    const status =
        latest.status || "UNKNOWN";


    const statusElement =
        element(
            "statusValue"
        );


    statusElement.textContent =
        status;


    statusElement.classList.remove(
        "status-online",
        "status-warning",
        "status-critical",
    );


    statusElement.classList.add(
        statusClass(status)
    );


    setText(
        "statusSSID",
        `SSID: ${latest.ssid || "--"}`
    );


    /*
     * Device / IP.
     */
    setText(
        "deviceIp",
        latest.ip_address ||
        "Unknown"
    );


    setText(
        "deviceIdValue",
        `Device: ${
            latest.device_id || "--"
        }`
    );


    /*
     * RSSI.
     */
    setText(
        "rssiValue",
        latest.rssi_dbm == null
            ? "--"
            : `${latest.rssi_dbm} dBm`
    );


    setText(
        "rssiDetail",
        "RSSI"
    );


    /*
     * Internet RTT.
     */
    setText(
        "rttValue",
        formatMs(
            latest.internet_rtt_ms
        )
    );


    setText(
        "rttStats",
        (
            `Min ${formatMs(
                latest.min_rtt_ms
            )} / ` +
            `Avg ${formatMs(
                latest.avg_rtt_ms == null
                    ? null
                    : Number(
                        latest.avg_rtt_ms
                    ).toFixed(1)
            )} / ` +
            `Max ${formatMs(
                latest.max_rtt_ms
            )}`
        )
    );


    /*
     * DNS.
     */
    setText(
        "dnsLatency",
        formatMs(
            latest.dns_latency_ms
        )
    );


    const resolver =
        latest.dns_server ||
        "Unknown";


    const testDomain =
        latest.dns_test_domain ||
        "--";


    setText(
        "dnsServer",
        `Resolver: ${resolver}`
    );


    setText(
        "dnsDomain",
        `Query: ${testDomain}`
    );


    setText(
        "dnsChartServer",
        (
            `Resolver: ${resolver} • ` +
            `Query: ${testDomain}`
        )
    );


    /*
     * Jitter.
     */
    setText(
        "jitterValue",
        formatMs(
            latest.jitter_ms
        )
    );


    setText(
        "jitterAverage",
        (
            "Average: " +
            (
                latest.avg_jitter_ms ==
                null
                    ? "--"
                    : `${
                        Number(
                            latest
                                .avg_jitter_ms
                        ).toFixed(1)
                    } ms`
            )
        )
    );


    /*
     * Packet loss.
     */
    setText(
        "lossValue",
        formatPercent(
            latest.packet_loss_pct
        )
    );


    /*
     * Access point.
     */
    setText(
        "channelValue",
        latest.channel == null
            ? "--"
            : `Channel ${latest.channel}`
    );


    setText(
        "bssidValue",
        `BSSID: ${
            latest.bssid || "--"
        }`
    );


    /*
     * Gateway.
     */
    setText(
        "gatewayRtt",
        formatMs(
            latest.gateway_rtt_ms
        )
    );


    setText(
        "gatewayValue",
        `Gateway: ${
            latest.gateway || "--"
        }`
    );


    /*
     * Reconnects.
     */
    setText(
        "reconnectValue",
        latest.reconnects ?? "--"
    );


    setText(
        "disconnectValue",
        (
            `Disconnects: ${
                latest.disconnects ?? "--"
            }`
        )
    );
}


/*
 * ================================================================
 * Historical graphs
 * ================================================================
 */

function updateChart(
    chart,
    labels,
    values,
) {
    chart.data.labels =
        labels;


    chart.data.datasets[0].data =
        values;


    chart.update(
        "none"
    );
}


async function updateMeasurements() {
    if (!selectedDevice) {
        return;
    }


    const data = await getJson(
        apiUrl(
            "/api/measurements",
            {
                limit: HISTORY_LIMIT
            }
        )
    );


    const measurements =
        data.measurements || [];


    const labels =
        measurements.map(
            measurement =>
                formatTime(
                    measurement.received_at
                )
        );


    updateChart(
        rttChart,
        labels,
        measurements.map(
            measurement =>
                measurement
                    .internet_rtt_ms
        )
    );


    updateChart(
        dnsLatencyChart,
        labels,
        measurements.map(
            measurement =>
                measurement
                    .dns_latency_ms
        )
    );


    updateChart(
        rssiChart,
        labels,
        measurements.map(
            measurement =>
                measurement.rssi_dbm
        )
    );


    updateChart(
        jitterChart,
        labels,
        measurements.map(
            measurement =>
                measurement.jitter_ms
        )
    );


    updateChart(
        lossChart,
        labels,
        measurements.map(
            measurement =>
                measurement
                    .packet_loss_pct
        )
    );
}


/*
 * ================================================================
 * Event filters
 * ================================================================
 */

async function updateEventTypes() {
    if (!selectedDevice) {
        return;
    }


    const data = await getJson(
        apiUrl(
            "/api/event-types"
        )
    );


    const types =
        data.event_types || [];


    const selector =
        element(
            "eventTypeFilter"
        );


    const previous =
        selector.value;


    selector.innerHTML =
        '<option value="">All event types</option>';


    for (const type of types) {
        const option =
            document.createElement(
                "option"
            );

        option.value =
            type;

        option.textContent =
            type;

        selector.appendChild(
            option
        );
    }


    if (
        types.includes(previous)
    ) {
        selector.value =
            previous;
    }
}


/*
 * ================================================================
 * Event log
 * ================================================================
 */

function formatEventDetails(
    details
) {
    if (
        !details ||
        Object.keys(details).length === 0
    ) {
        return "";
    }


    if (
        Object.prototype
            .hasOwnProperty
            .call(
                details,
                "old"
            )
        &&
        Object.prototype
            .hasOwnProperty
            .call(
                details,
                "new"
            )
    ) {
        return (
            `${details.old} → ` +
            `${details.new}`
        );
    }


    if (
        details.metric !== undefined &&
        details.value !== undefined
    ) {
        let output =
            `${details.metric}: ` +
            `${details.value}`;


        if (
            details.threshold !==
            undefined
        ) {
            output +=
                ` (threshold: ` +
                `${details.threshold})`;
        }


        return output;
    }


    if (
        details.duration_ms !==
        undefined
    ) {
        return (
            `${
                (
                    details.duration_ms /
                    1000
                ).toFixed(2)
            } seconds`
        );
    }


    return JSON.stringify(
        details
    );
}


async function updateEvents() {
    if (!selectedDevice) {
        return;
    }


    const severity =
        element(
            "severityFilter"
        ).value;


    const eventType =
        element(
            "eventTypeFilter"
        ).value;


    const data = await getJson(
        apiUrl(
            "/api/events",
            {
                limit:
                    EVENT_LIMIT,

                severity:
                    severity,

                event_type:
                    eventType,
            }
        )
    );


    const events =
        data.events || [];


    const body =
        element(
            "eventTableBody"
        );


    body.innerHTML = "";


    if (
        events.length === 0
    ) {
        const row =
            document.createElement(
                "tr"
            );


        row.innerHTML =
            `
            <td
                colspan="4"
                class="empty-row"
            >
                No matching events.
            </td>
            `;


        body.appendChild(
            row
        );


        return;
    }


    for (
        const event
        of events
    ) {
        const row =
            document.createElement(
                "tr"
            );


        const timeCell =
            document.createElement(
                "td"
            );


        timeCell.textContent =
            formatTime(
                event.received_at
            );


        const severityCell =
            document.createElement(
                "td"
            );


        const severityText =
            event.severity ||
            "unknown";


        severityCell.textContent =
            severityText.toUpperCase();


        severityCell.className =
            `severity-${severityText}`;


        const eventCell =
            document.createElement(
                "td"
            );


        eventCell.textContent =
            event.event_type ||
            "UNKNOWN";


        const detailsCell =
            document.createElement(
                "td"
            );


        detailsCell.textContent =
            formatEventDetails(
                event.details
            );


        row.append(
            timeCell,
            severityCell,
            eventCell,
            detailsCell,
        );


        body.appendChild(
            row
        );
    }
}


/*
 * ================================================================
 * Dashboard refresh
 * ================================================================
 */

async function refreshDashboard() {
    if (!selectedDevice) {
        return;
    }


    try {
        await Promise.all([
            updateStatus(),
            updateMeasurements(),
            updateEvents(),
        ]);


        setText(
            "lastUpdated",
            (
                "Last updated: " +
                new Date()
                    .toLocaleTimeString()
            )
        );


        setText(
            "dashboardError",
            ""
        );
    }
    catch (error) {
        console.error(
            error
        );


        setText(
            "dashboardError",
            `Error: ${error.message}`
        );
    }
}


/*
 * ================================================================
 * Event listeners
 * ================================================================
 */

element(
    "deviceSelector"
).addEventListener(
    "change",
    async event => {
        selectedDevice =
            event.target.value ||
            null;


        updateExportLinks();


        await updateEventTypes();
        await refreshDashboard();
    }
);


element(
    "severityFilter"
).addEventListener(
    "change",
    updateEvents
);


element(
    "eventTypeFilter"
).addEventListener(
    "change",
    updateEvents
);


/*
 * ================================================================
 * Startup
 * ================================================================
 */

async function startDashboard() {
    try {
        await loadDevices();

        await updateEventTypes();

        await refreshDashboard();


        /*
         * Live measurements.
         */
        setInterval(
            refreshDashboard,
            POLL_INTERVAL_MS
        );


        /*
         * Devices may appear later.
         */
        setInterval(
            async () => {
                try {
                    await loadDevices();
                }
                catch (error) {
                    console.error(
                        error
                    );
                }
            },
            30000
        );


        /*
         * Newly created event types should
         * automatically enter the filter.
         */
        setInterval(
            async () => {
                try {
                    await updateEventTypes();
                }
                catch (error) {
                    console.error(
                        error
                    );
                }
            },
            30000
        );
    }
    catch (error) {
        console.error(
            error
        );


        setText(
            "dashboardError",
            `Error: ${error.message}`
        );
    }
}


startDashboard();
