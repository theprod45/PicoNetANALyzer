#!/usr/bin/env python3

import argparse
import json
import sqlite3
import sys
import time

from datetime import datetime, timezone
from pathlib import Path

import serial
from serial import SerialException


# Collector currently understands telemetry schema version 1.
SUPPORTED_SCHEMA_VERSION = 1


# ---------------------------------------------------------------------------
# Time
# ---------------------------------------------------------------------------

def utc_now() -> str:
    """
    Return the current UTC timestamp in ISO-8601 format.

    Example:
        2026-08-08T15:42:31.123456+00:00
    """
    return datetime.now(timezone.utc).isoformat()


# ---------------------------------------------------------------------------
# Database setup
# ---------------------------------------------------------------------------

def create_database(database_path: Path) -> sqlite3.Connection:
    """
    Open/create the SQLite database and initialize all required tables.

    The database contains:

        records
            Every raw JSON message received from the Pico.

        measurements
            Structured network measurements.

        events
            Structured network/security events.

    Keeping the raw JSON in records makes the collector easier to extend.
    If future firmware sends fields that this collector does not yet know
    about, those fields are still preserved.
    """

    database_path.parent.mkdir(
        parents=True,
        exist_ok=True
    )

    connection = sqlite3.connect(
        database_path
    )

    # Better behavior when the dashboard reads while the collector writes.
    connection.execute(
        "PRAGMA journal_mode=WAL"
    )

    connection.execute(
        "PRAGMA foreign_keys=ON"
    )

    connection.executescript(
        """
        CREATE TABLE IF NOT EXISTS records (
            id INTEGER PRIMARY KEY AUTOINCREMENT,

            received_at TEXT NOT NULL,

            schema_version INTEGER NOT NULL,

            device_id TEXT NOT NULL,

            record_type TEXT NOT NULL,

            uptime_ms INTEGER,

            payload_json TEXT NOT NULL
        );


        CREATE TABLE IF NOT EXISTS measurements (
            record_id INTEGER PRIMARY KEY,

            received_at TEXT NOT NULL,

            device_id TEXT NOT NULL,

            uptime_ms INTEGER,

            status TEXT,

            ssid TEXT,
            bssid TEXT,

            channel INTEGER,
            rssi_dbm INTEGER,

            gateway TEXT,

            gateway_rtt_ms INTEGER,
            gateway_loss_pct REAL,

            internet_rtt_ms INTEGER,

            min_rtt_ms INTEGER,
            avg_rtt_ms REAL,
            max_rtt_ms INTEGER,

            jitter_ms INTEGER,
            avg_jitter_ms REAL,

            packet_loss_pct REAL,

            samples INTEGER,
            successful INTEGER,
            failed INTEGER,

            disconnects INTEGER,
            reconnects INTEGER,
            reconnect_attempts INTEGER,

            bssid_changes INTEGER,
            channel_changes INTEGER,
            gateway_changes INTEGER,

            weak_signal_events INTEGER,
            high_latency_events INTEGER,

            weak_signal_active INTEGER,
            high_latency_active INTEGER,

            FOREIGN KEY(record_id)
                REFERENCES records(id)
                ON DELETE CASCADE
        );


        CREATE TABLE IF NOT EXISTS events (
            record_id INTEGER PRIMARY KEY,

            received_at TEXT NOT NULL,

            device_id TEXT NOT NULL,

            uptime_ms INTEGER,

            event_type TEXT NOT NULL,

            severity TEXT,

            details_json TEXT,

            FOREIGN KEY(record_id)
                REFERENCES records(id)
                ON DELETE CASCADE
        );


        CREATE INDEX IF NOT EXISTS idx_records_received_at
            ON records(received_at);


        CREATE INDEX IF NOT EXISTS idx_records_device
            ON records(device_id);


        CREATE INDEX IF NOT EXISTS idx_measurements_received_at
            ON measurements(received_at);


        CREATE INDEX IF NOT EXISTS idx_measurements_device
            ON measurements(device_id);


        CREATE INDEX IF NOT EXISTS idx_measurements_status
            ON measurements(status);


        CREATE INDEX IF NOT EXISTS idx_events_received_at
            ON events(received_at);


        CREATE INDEX IF NOT EXISTS idx_events_device
            ON events(device_id);


        CREATE INDEX IF NOT EXISTS idx_events_type
            ON events(event_type);


        CREATE INDEX IF NOT EXISTS idx_events_severity
            ON events(severity);
        """
    )

    connection.commit()

    return connection


# ---------------------------------------------------------------------------
# Generic raw record storage
# ---------------------------------------------------------------------------

def insert_raw_record(
    connection: sqlite3.Connection,
    received_at: str,
    message: dict
) -> int:
    """
    Store the complete JSON message.

    Returns the SQLite record ID.
    """

    payload_json = json.dumps(
        message,
        separators=(",", ":")
    )

    cursor = connection.execute(
        """
        INSERT INTO records (
            received_at,
            schema_version,
            device_id,
            record_type,
            uptime_ms,
            payload_json
        )
        VALUES (?, ?, ?, ?, ?, ?)
        """,
        (
            received_at,

            message.get(
                "schema",
                0
            ),

            message.get(
                "device_id",
                "unknown"
            ),

            message.get(
                "type",
                "unknown"
            ),

            message.get(
                "uptime_ms"
            ),

            payload_json,
        )
    )

    return cursor.lastrowid


# ---------------------------------------------------------------------------
# Measurement storage
# ---------------------------------------------------------------------------

def insert_measurement(
    connection: sqlite3.Connection,
    record_id: int,
    received_at: str,
    message: dict
) -> None:
    """
    Store a structured measurement record.
    """

    connection.execute(
        """
        INSERT INTO measurements (
            record_id,
            received_at,
            device_id,
            uptime_ms,

            status,

            ssid,
            bssid,

            channel,
            rssi_dbm,

            gateway,

            gateway_rtt_ms,
            gateway_loss_pct,

            internet_rtt_ms,

            min_rtt_ms,
            avg_rtt_ms,
            max_rtt_ms,

            jitter_ms,
            avg_jitter_ms,

            packet_loss_pct,

            samples,
            successful,
            failed,

            disconnects,
            reconnects,
            reconnect_attempts,

            bssid_changes,
            channel_changes,
            gateway_changes,

            weak_signal_events,
            high_latency_events,

            weak_signal_active,
            high_latency_active
        )
        VALUES (
            ?, ?, ?, ?,
            ?,
            ?, ?,
            ?, ?,
            ?,
            ?, ?,
            ?,
            ?, ?, ?,
            ?, ?,
            ?,
            ?, ?, ?,
            ?, ?, ?,
            ?, ?, ?,
            ?, ?,
            ?, ?
        )
        """,
        (
            record_id,
            received_at,

            message.get(
                "device_id",
                "unknown"
            ),

            message.get(
                "uptime_ms"
            ),

            message.get(
                "status"
            ),

            message.get(
                "ssid"
            ),

            message.get(
                "bssid"
            ),

            message.get(
                "channel"
            ),

            message.get(
                "rssi_dbm"
            ),

            message.get(
                "gateway"
            ),

            message.get(
                "gateway_rtt_ms"
            ),

            message.get(
                "gateway_loss_pct"
            ),

            message.get(
                "internet_rtt_ms"
            ),

            message.get(
                "min_rtt_ms"
            ),

            message.get(
                "avg_rtt_ms"
            ),

            message.get(
                "max_rtt_ms"
            ),

            message.get(
                "jitter_ms"
            ),

            message.get(
                "avg_jitter_ms"
            ),

            message.get(
                "packet_loss_pct"
            ),

            message.get(
                "samples"
            ),

            message.get(
                "successful"
            ),

            message.get(
                "failed"
            ),

            message.get(
                "disconnects"
            ),

            message.get(
                "reconnects"
            ),

            message.get(
                "reconnect_attempts"
            ),

            message.get(
                "bssid_changes"
            ),

            message.get(
                "channel_changes"
            ),

            message.get(
                "gateway_changes"
            ),

            message.get(
                "weak_signal_events"
            ),

            message.get(
                "high_latency_events"
            ),

            bool_to_int(
                message.get(
                    "weak_signal_active"
                )
            ),

            bool_to_int(
                message.get(
                    "high_latency_active"
                )
            ),
        )
    )


# ---------------------------------------------------------------------------
# Event storage
# ---------------------------------------------------------------------------

def insert_event(
    connection: sqlite3.Connection,
    record_id: int,
    received_at: str,
    message: dict
) -> None:
    """
    Store a structured network/security event.

    details_json is deliberately flexible.

    Examples:

        BSSID_CHANGED
        {
            "old": "...",
            "new": "..."
        }

        HIGH_LATENCY
        {
            "metric": "internet_rtt_ms",
            "value": 352,
            "threshold": 150
        }
    """

    details = message.get(
        "details",
        {}
    )

    connection.execute(
        """
        INSERT INTO events (
            record_id,
            received_at,
            device_id,
            uptime_ms,
            event_type,
            severity,
            details_json
        )
        VALUES (?, ?, ?, ?, ?, ?, ?)
        """,
        (
            record_id,

            received_at,

            message.get(
                "device_id",
                "unknown"
            ),

            message.get(
                "uptime_ms"
            ),

            message.get(
                "event",
                "UNKNOWN_EVENT"
            ),

            message.get(
                "severity",
                "info"
            ),

            json.dumps(
                details,
                separators=(",", ":")
            ),
        )
    )


# ---------------------------------------------------------------------------
# Utility
# ---------------------------------------------------------------------------

def bool_to_int(value) -> int:
    """
    Convert JSON boolean values to SQLite integer booleans.
    """

    if value is True:
        return 1

    return 0


# ---------------------------------------------------------------------------
# Incoming message processing
# ---------------------------------------------------------------------------

def process_message(
    connection: sqlite3.Connection,
    message: dict,
    verbose: bool = False
) -> None:
    """
    Process one complete JSON message from the Pico.
    """

    received_at = utc_now()


    # -----------------------------------------------------------------------
    # Basic validation
    # -----------------------------------------------------------------------

    schema_version = message.get(
        "schema"
    )

    if schema_version is None:
        print(
            "[WARNING] Ignoring message without schema field",
            file=sys.stderr
        )

        return


    if not isinstance(
        schema_version,
        int
    ):
        print(
            "[WARNING] Invalid schema version",
            file=sys.stderr
        )

        return


    if schema_version > SUPPORTED_SCHEMA_VERSION:
        print(
            "[WARNING] Pico firmware is using "
            f"schema {schema_version}, but collector "
            f"only explicitly understands schema "
            f"{SUPPORTED_SCHEMA_VERSION}. "
            "Raw JSON will still be preserved.",
            file=sys.stderr
        )


    record_type = message.get(
        "type"
    )


    if not record_type:
        print(
            "[WARNING] Ignoring record without type",
            file=sys.stderr
        )

        return


    # -----------------------------------------------------------------------
    # Store complete original record
    # -----------------------------------------------------------------------

    record_id = insert_raw_record(
        connection,
        received_at,
        message
    )


    # -----------------------------------------------------------------------
    # Measurement
    # -----------------------------------------------------------------------

    if record_type == "measurement":

        insert_measurement(
            connection,
            record_id,
            received_at,
            message
        )


        if verbose:

            print(
                "[MEASUREMENT] "
                f"{received_at}  "
                f"device={message.get('device_id')}  "
                f"status={message.get('status')}  "
                f"RSSI={message.get('rssi_dbm')} dBm  "
                f"RTT={message.get('internet_rtt_ms')} ms  "
                f"jitter={message.get('jitter_ms')} ms  "
                f"loss={message.get('packet_loss_pct')}%"
            )


    # -----------------------------------------------------------------------
    # Event
    # -----------------------------------------------------------------------

    elif record_type == "event":

        insert_event(
            connection,
            record_id,
            received_at,
            message
        )


        print(
            "[EVENT] "
            f"{received_at}  "
            f"{message.get('severity', 'info').upper()}  "
            f"{message.get('event', 'UNKNOWN_EVENT')}  "
            f"{message.get('details', {})}"
        )


    # -----------------------------------------------------------------------
    # Unknown future record type
    # -----------------------------------------------------------------------

    else:

        print(
            "[INFO] Unknown record type "
            f"'{record_type}' stored in raw records table."
        )


    connection.commit()


# ---------------------------------------------------------------------------
# Serial reader
# ---------------------------------------------------------------------------

def collect_serial(
    connection: sqlite3.Connection,
    port: str,
    baud: int,
    verbose: bool
) -> None:
    """
    Open the Pico serial device and continuously process NDJSON records.

    If the Pico disconnects/reboots, the collector automatically tries
    to reconnect.
    """

    while True:

        try:

            print(
                f"[SERIAL] Opening {port} @ {baud}..."
            )


            with serial.Serial(
                port=port,
                baudrate=baud,
                timeout=1
            ) as device:

                print(
                    "[SERIAL] Connected."
                )


                while True:

                    raw_bytes = (
                        device.readline()
                    )


                    if not raw_bytes:
                        continue


                    raw_line = (
                        raw_bytes
                        .decode(
                            "utf-8",
                            errors="replace"
                        )
                        .strip()
                    )


                    if not raw_line:
                        continue


                                     # Pico telemetry records always begin
                    # with a JSON object.
                    #
                    # This lets us safely ignore any startup
                    # noise or legacy human-readable serial output.

                    if not raw_line.startswith("{"):

                        if verbose:
                            print(
                                f"[SERIAL RAW] {raw_line}"
                            )

                        continue


                    # -------------------------------------------------------
                    # Parse JSON
                    # -------------------------------------------------------

                    try:

                        message = json.loads(
                            raw_line
                        )


                    except json.JSONDecodeError as error:

                        print(
                            "[WARNING] Invalid JSON received:",
                            file=sys.stderr
                        )

                        print(
                            f"          {error}",
                            file=sys.stderr
                        )

                        print(
                            f"          {raw_line}",
                            file=sys.stderr
                        )

                        continue


                    if not isinstance(
                        message,
                        dict
                    ):
                        print(
                            "[WARNING] JSON record is not an object",
                            file=sys.stderr
                        )

                        continue


                    # -------------------------------------------------------
                    # Store message
                    # -------------------------------------------------------

                    process_message(
                        connection,
                        message,
                        verbose
                    )


        except SerialException as error:

            print(
                f"[SERIAL] Connection lost: {error}",
                file=sys.stderr
            )

            print(
                "[SERIAL] Retrying in 3 seconds..."
            )

            time.sleep(3)


# ---------------------------------------------------------------------------
# Command-line arguments
# ---------------------------------------------------------------------------

def parse_arguments():
    """
    Parse collector command-line options.
    """

    parser = argparse.ArgumentParser(
        description=(
            "PicoNetANALyzer telemetry collector. "
            "Reads NDJSON telemetry from the Pico over "
            "USB serial and stores it in SQLite."
        )
    )


    parser.add_argument(
        "--port",
        default="/dev/ttyACM0",
        help=(
            "Pico serial device "
            "(default: /dev/ttyACM0)"
        )
    )


    parser.add_argument(
        "--baud",
        type=int,
        default=115200,
        help=(
            "Serial baud rate "
            "(default: 115200)"
        )
    )


    parser.add_argument(
        "--db",
        default="data/piconet.db",
        help=(
            "SQLite database path "
            "(default: data/piconet.db)"
        )
    )


    parser.add_argument(
        "--verbose",
        action="store_true",
        help=(
            "Print every measurement received "
            "instead of only events"
        )
    )


    return parser.parse_args()


# ---------------------------------------------------------------------------
# Main
# ---------------------------------------------------------------------------

def main():
    """
    Collector entry point.
    """

    args = parse_arguments()


    database_path = Path(
        args.db
    )


    connection = create_database(
        database_path
    )


    print()
    print("PicoNetANALyzer Collector")
    print("-------------------------")

    print(
        f"Serial Port: {args.port}"
    )

    print(
        f"Baud Rate:   {args.baud}"
    )

    print(
        f"Database:    {database_path}"
    )

    print()


    try:

        collect_serial(
            connection=connection,
            port=args.port,
            baud=args.baud,
            verbose=args.verbose
        )


    except KeyboardInterrupt:

        print()
        print(
            "Stopping collector..."
        )


    finally:

        connection.commit()
        connection.close()

        print(
            "Database closed."
        )


if __name__ == "__main__":
    main()
