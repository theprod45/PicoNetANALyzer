"use strict";


const POLL_INTERVAL_MS = 2000;
const HISTORY_LIMIT = 180;
const EVENT_LIMIT = 100;


let selectedDevice = "";

let rttChart = null;
let rssiChart = null;
let jitterChart = null;
let lossChart = null;


// ---------------------------------------------------------------------------
// DOM helpers
// ---------------------------------------------------------------------------

function element(id)
{
    return document.getElementById(id);
}


function setText(id, value)
{
    element(id).textContent = value;
}


// ---------------------------------------------------------------------------
// Formatting
// ---------------------------------------------------------------------------

function formatValue(
    value,
    suffix = "",
    decimals = null
)
{
    if (
        value === null ||
        value === undefined
    )
    {
        return "--";
    }


    if (
        decimals !== null &&
        typeof value === "number"
    )
    {
        return (
            value.toFixed(decimals)
            + suffix
        );
    }


    return `${value}${suffix}`;
}


function formatTime(value)
{
    if (!value)
    {
        return "--";
    }


    const date = new Date(value);


    return date.toLocaleString();
}


function shortTime(value)
{
    if (!value)
    {
        return "";
    }


    const date = new Date(value);


    return date.toLocaleTimeString(
        [],
        {
            hour: "2-digit",
            minute: "2-digit",
            second: "2-digit"
        }
    );
}


// ---------------------------------------------------------------------------
// API
// ---------------------------------------------------------------------------

function buildUrl(
    path,
    parameters = {}
)
{
    const url =
        new URL(
            path,
            window.location.origin
        );


    if (selectedDevice)
    {
        url.searchParams.set(
            "device_id",
            selectedDevice
        );
    }


    for (
        const [key, value]
        of Object.entries(parameters)
    )
    {
        if (
            value !== null &&
            value !== undefined &&
            value !== ""
        )
        {
            url.searchParams.set(
                key,
                value
            );
        }
    }


    return url.toString();
}


async function getJson(
    path,
    parameters = {}
)
{
    const response =
        await fetch(
            buildUrl(
                path,
                parameters
            ),
            {
                cache: "no-store"
            }
        );


    if (!response.ok)
    {
        throw new Error(
            `${response.status} ${response.statusText}`
        );
    }


    return response.json();
}


// ---------------------------------------------------------------------------
// Devices
// ---------------------------------------------------------------------------

async function loadDevices()
{
    const result =
        await getJson(
            "/api/devices"
        );


    const selector =
        element(
            "device-selector"
        );


    const oldDevice =
        selectedDevice;


    selector.innerHTML = "";


    if (
        !result.devices ||
        result.devices.length === 0
    )
    {
        const option =
            document.createElement(
                "option"
            );


        option.textContent =
            "No devices";


        selector.appendChild(
            option
        );


        return;
    }


    for (
        const device
        of result.devices
    )
    {
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
        oldDevice &&
        result.devices.some(
            device =>
                device.device_id
                === oldDevice
        )
    )
    {
        selector.value =
            oldDevice;
    }


    selectedDevice =
        selector.value;


    updateExportLinks();
}


// ---------------------------------------------------------------------------
// Export links
// ---------------------------------------------------------------------------

function updateExportLinks()
{
    element(
        "export-measurements"
    ).href =
        buildUrl(
            "/api/export/measurements.csv"
        );


    element(
        "export-events"
    ).href =
        buildUrl(
            "/api/export/events.csv"
        );
}


// ---------------------------------------------------------------------------
// Current status
// ---------------------------------------------------------------------------

async function updateStatus()
{
    const result =
        await getJson(
            "/api/status"
        );


    if (
        !result.available ||
        !result.measurement
    )
    {
        setText(
            "status-value",
            "NO DATA"
        );

        return;
    }


    const m =
        result.measurement;


    setText(
        "status-value",
        m.status || "--"
    );


    const statusElement =
        element(
            "status-value"
        );


    statusElement.className =
        "metric-value";


    if (m.status === "ONLINE")
    {
        statusElement.classList.add(
            "status-online"
        );
    }
    else if (
        m.status ===
            "DISCONNECTED"
        ||
        m.status ===
            "INTERNET_ISSUE"
        ||
        m.status ===
            "LOCAL_NETWORK_ISSUE"
    )
    {
        statusElement.classList.add(
            "status-danger"
        );
    }
    else
    {
        statusElement.classList.add(
            "status-warning"
        );
    }


    setText(
        "status-time",
        `Last sample ${formatTime(
            m.received_at
        )}`
    );


    setText(
        "rssi-value",
        formatValue(
            m.rssi_dbm,
            " dBm"
        )
    );


    setText(
        "rtt-value",
        formatValue(
            m.internet_rtt_ms,
            " ms"
        )
    );


    setText(
        "rtt-range",
        (
            `${formatValue(
                m.min_rtt_ms
            )} / `
            +
            `${formatValue(
                m.avg_rtt_ms,
                "",
                1
            )} / `
            +
            `${formatValue(
                m.max_rtt_ms
            )} ms`
        )
    );


    setText(
        "jitter-value",
        formatValue(
            m.jitter_ms,
            " ms"
        )
    );


    setText(
        "avg-jitter",
        (
            "Average "
            +
            formatValue(
                m.avg_jitter_ms,
                " ms",
                1
            )
        )
    );


    setText(
        "loss-value",
        formatValue(
            m.packet_loss_pct,
            "%",
            2
        )
    );


    setText(
        "channel-value",
        formatValue(
            m.channel
        )
    );


    setText(
        "bssid-value",
        `BSSID ${m.bssid || "--"}`
    );


    setText(
        "gateway-rtt-value",
        formatValue(
            m.gateway_rtt_ms,
            " ms"
        )
    );


    setText(
        "gateway-value",
        `Gateway ${m.gateway || "--"}`
    );


    setText(
        "disconnect-value",
        formatValue(
            m.disconnects
        )
    );


    setText(
        "reconnect-value",
        (
            "Reconnects "
            +
            formatValue(
                m.reconnects
            )
        )
    );
}


// ---------------------------------------------------------------------------
// Charts
// ---------------------------------------------------------------------------

function createLineChart(
    canvasId,
    label,
    color
)
{
    return new Chart(
        element(canvasId),
        {
            type: "line",

            data:
            {
                labels: [],

                datasets:
                [
                    {
                        label: label,

                        data: [],

                        borderColor: color,

                        backgroundColor: color,

                        borderWidth: 2,

                        pointRadius: 0,

                        tension: 0.2,

                        spanGaps: true
                    }
                ]
            },

            options:
            {
                responsive: true,

                maintainAspectRatio: false,

                animation: false,

                interaction:
                {
                    intersect: false,
                    mode: "index"
                },

                plugins:
                {
                    legend:
                    {
                        display: false
                    }
                },

                scales:
                {
                    x:
                    {
                        ticks:
                        {
                            maxTicksLimit: 8,
                            color: "#8b949e"
                        },

                        grid:
                        {
                            color:
                                "rgba(139,148,158,0.08)"
                        }
                    },

                    y:
                    {
                        ticks:
                        {
                            color: "#8b949e"
                        },

                        grid:
                        {
                            color:
                                "rgba(139,148,158,0.08)"
                        }
                    }
                }
            }
        }
    );
}


function initializeCharts()
{
    rttChart =
        createLineChart(
            "rtt-chart",
            "Internet RTT",
            "#58a6ff"
        );


    rssiChart =
        createLineChart(
            "rssi-chart",
            "RSSI",
            "#3fb950"
        );


    jitterChart =
        createLineChart(
            "jitter-chart",
            "Jitter",
            "#d29922"
        );


    lossChart =
        createLineChart(
            "loss-chart",
            "Packet Loss",
            "#f85149"
        );
}


function updateChart(
    chart,
    labels,
    values
)
{
    chart.data.labels =
        labels;


    chart.data.datasets[0].data =
        values;


    chart.update(
        "none"
    );
}


async function updateMeasurements()
{
    const result =
        await getJson(
            "/api/measurements",
            {
                limit: HISTORY_LIMIT
            }
        );


    const measurements =
        result.measurements || [];


    const labels =
        measurements.map(
            measurement =>
                shortTime(
                    measurement.received_at
                )
        );


    updateChart(
        rttChart,
        labels,
        measurements.map(
            measurement =>
                measurement.internet_rtt_ms
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
                measurement.packet_loss_pct
        )
    );
}


// ---------------------------------------------------------------------------
// Event filters
// ---------------------------------------------------------------------------

async function updateEventTypes()
{
    const result =
        await getJson(
            "/api/event-types"
        );


    const selector =
        element(
            "event-filter"
        );


    const previous =
        selector.value;


    selector.innerHTML =
        '<option value="">All event types</option>';


    for (
        const eventType
        of result.event_types || []
    )
    {
        const option =
            document.createElement(
                "option"
            );


        option.value =
            eventType.event_type;


        option.textContent =
            (
                `${eventType.event_type} `
                +
                `(${eventType.count})`
            );


        selector.appendChild(
            option
        );
    }


    if (
        [...selector.options].some(
            option =>
                option.value
                === previous
        )
    )
    {
        selector.value =
            previous;
    }
}


// ---------------------------------------------------------------------------
// Event display
// ---------------------------------------------------------------------------

function formatEventDetails(
    details
)
{
    if (!details)
    {
        return "";
    }


    if (
        Object.prototype
            .hasOwnProperty.call(
                details,
                "old"
            )
        &&
        Object.prototype
            .hasOwnProperty.call(
                details,
                "new"
            )
    )
    {
        return (
            `${details.old}`
            +
            " → "
            +
            `${details.new}`
        );
    }


    if (
        details.metric !== undefined
    )
    {
        return (
            `${details.metric}: `
            +
            `${details.value}`
            +
            ` (threshold `
            +
            `${details.threshold})`
        );
    }


    if (
        details.duration_ms !== undefined
    )
    {
        return (
            `${(
                details.duration_ms
                / 1000
            ).toFixed(2)} seconds`
        );
    }


    return JSON.stringify(
        details
    );
}


async function updateEvents()
{
    const severity =
        element(
            "severity-filter"
        ).value;


    const eventType =
        element(
            "event-filter"
        ).value;


    const result =
        await getJson(
            "/api/events",
            {
                limit: EVENT_LIMIT,
                severity: severity,
                event_type: eventType
            }
        );


    const table =
        element(
            "event-table"
        );


    table.innerHTML = "";


    if (
        !result.events ||
        result.events.length === 0
    )
    {
        const row =
            document.createElement(
                "tr"
            );


        row.innerHTML =
            '<td colspan="4">No events found.</td>';


        table.appendChild(
            row
        );


        return;
    }


    for (
        const event
        of result.events
    )
    {
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


        severityCell.textContent =
            (
                event.severity ||
                "info"
            ).toUpperCase();


        severityCell.className =
            (
                "severity-"
                +
                (
                    event.severity ||
                    "info"
                )
            );


        const eventCell =
            document.createElement(
                "td"
            );


        eventCell.textContent =
            event.event_type;


        const detailCell =
            document.createElement(
                "td"
            );


        detailCell.className =
            "details";


        detailCell.textContent =
            formatEventDetails(
                event.details
            );


        row.appendChild(
            timeCell
        );


        row.appendChild(
            severityCell
        );


        row.appendChild(
            eventCell
        );


        row.appendChild(
            detailCell
        );


        table.appendChild(
            row
        );
    }
}


// ---------------------------------------------------------------------------
// Refresh
// ---------------------------------------------------------------------------

function showError(error)
{
    const errorElement =
        element(
            "error"
        );


    errorElement.style.display =
        "block";


    errorElement.textContent =
        `Dashboard error: ${error.message}`;


    setText(
        "live-status",
        "Connection error"
    );
}


function clearError()
{
    element(
        "error"
    ).style.display =
        "none";


    setText(
        "live-status",
        "Live"
    );
}


async function refreshDashboard()
{
    try
    {
        await Promise.all(
            [
                updateStatus(),
                updateMeasurements(),
                updateEvents()
            ]
        );


        clearError();


        setText(
            "last-refresh",
            (
                "Dashboard refreshed "
                +
                new Date()
                    .toLocaleTimeString()
            )
        );
    }
    catch (error)
    {
        showError(
            error
        );
    }
}


// ---------------------------------------------------------------------------
// Device changes
// ---------------------------------------------------------------------------

async function deviceChanged()
{
    selectedDevice =
        element(
            "device-selector"
        ).value;


    updateExportLinks();


    await updateEventTypes();


    await refreshDashboard();
}


// ---------------------------------------------------------------------------
// Startup
// ---------------------------------------------------------------------------

async function startDashboard()
{
    try
    {
        initializeCharts();


        await loadDevices();


        await updateEventTypes();


        await refreshDashboard();


        element(
            "device-selector"
        ).addEventListener(
            "change",
            deviceChanged
        );


        element(
            "severity-filter"
        ).addEventListener(
            "change",
            updateEvents
        );


        element(
            "event-filter"
        ).addEventListener(
            "change",
            updateEvents
        );


        setInterval(
            refreshDashboard,
            POLL_INTERVAL_MS
        );


        /*
         * Refresh device list occasionally so a newly
         * connected Pico automatically becomes visible.
         */
        setInterval(
            loadDevices,
            30000
        );
        
        setInterval(
			updateEventTypes,
			30000
		);
    }
    catch (error)
    {
        showError(
            error
        );
    }
}


document.addEventListener(
    "DOMContentLoaded",
    startDashboard
);
