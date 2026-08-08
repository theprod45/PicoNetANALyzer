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


PROJECT_ROOT = Path(__file__).resolve().parent.parent

DEFAULT_DB = PROJECT_ROOT / "data" / "piconet.db"
DEFAULT_PORT = "/dev/ttyACM0"
DEFAULT_BAUD = 115200


def utc_now():
    return (
        datetime.now(timezone.utc)
        .isoformat(timespec="milliseconds")
        .replace("+00:00", "Z")
    )


def open_database(path):
    path = Path(path)

    path.parent.mkdir(
        parents=True,
        exist_ok=True,
    )

    conn = sqlite3.connect(
        path,
        timeout=10,
    )

    conn.execute(
        "PRAGMA journal_mode=WAL"
    )

    conn.execute(
        "PRAGMA synchronous=NORMAL"
    )

    return conn


def create_schema(conn):
    conn.executescript(
        """
        CREATE TABLE IF NOT EXISTS records (
            id INTEGER PRIMARY KEY AUTOINCREMENT,
            received_at TEXT NOT NULL,
            schema_version INTEGER,
            device_id TEXT,
            record_type TEXT,
            uptime_ms INTEGER,
            payload_json TEXT NOT NULL
        );


        CREATE TABLE IF NOT EXISTS measurements (
            record_id INTEGER PRIMARY KEY,

            received_at TEXT NOT NULL,
            device_id TEXT,
            uptime_ms INTEGER,

            status TEXT,

            ssid TEXT,
            bssid TEXT,

            channel INTEGER,
            rssi_dbm INTEGER,

            ip_address TEXT,

            gateway TEXT,
            gateway_rtt_ms INTEGER,
            gateway_loss_pct REAL,

            dns_server TEXT,
            dns_test_domain TEXT,
            dns_latency_ms INTEGER,

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
        );


        CREATE TABLE IF NOT EXISTS events (
            record_id INTEGER PRIMARY KEY,

            received_at TEXT NOT NULL,
            device_id TEXT,
            uptime_ms INTEGER,

            event_type TEXT,
            severity TEXT,

            details_json TEXT,

            FOREIGN KEY(record_id)
                REFERENCES records(id)
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

    conn.commit()

    ensure_measurement_columns(
        conn
    )


def ensure_measurement_columns(conn):
    """
    Automatically upgrade an existing PicoNetANALyzer
    database without deleting historical data.
    """

    existing = {
        row[1]
        for row in conn.execute(
            "PRAGMA table_info(measurements)"
        )
    }

    required = {
        "ip_address": "TEXT",
        "dns_server": "TEXT",
        "dns_test_domain": "TEXT",
        "dns_latency_ms": "INTEGER",
    }

    for column, column_type in required.items():
        if column not in existing:
            print(
                f"[DB] Adding measurements.{column}"
            )

            conn.execute(
                f"""
                ALTER TABLE measurements
                ADD COLUMN {column} {column_type}
                """
            )

    conn.commit()


def insert_record(
    conn,
    record,
    received_at,
):
    payload_json = json.dumps(
        record,
        separators=(",", ":"),
        ensure_ascii=False,
    )

    cursor = conn.execute(
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
            record.get("schema"),
            record.get("device_id"),
            record.get("type"),
            record.get("uptime_ms"),
            payload_json,
        ),
    )

    record_id = cursor.lastrowid

    if record.get("type") == "measurement":
        insert_measurement(
            conn,
            record_id,
            record,
            received_at,
        )

    elif record.get("type") == "event":
        insert_event(
            conn,
            record_id,
            record,
            received_at,
        )

    conn.commit()


def bool_to_int(value):
    if value is None:
        return None

    return 1 if value else 0


def insert_measurement(
    conn,
    record_id,
    record,
    received_at,
):
    conn.execute(
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

            ip_address,

            gateway,
            gateway_rtt_ms,
            gateway_loss_pct,

            dns_server,
            dns_test_domain,
            dns_latency_ms,

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
            ?, ?, ?, ?,
            ?,
            ?, ?, ?,
            ?, ?, ?,
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
            record.get("device_id"),
            record.get("uptime_ms"),

            record.get("status"),

            record.get("ssid"),
            record.get("bssid"),
            record.get("channel"),
            record.get("rssi_dbm"),

            record.get("ip_address"),

            record.get("gateway"),
            record.get("gateway_rtt_ms"),
            record.get("gateway_loss_pct"),

            record.get("dns_server"),
            record.get("dns_test_domain"),
            record.get("dns_latency_ms"),

            record.get("internet_rtt_ms"),

            record.get("min_rtt_ms"),
            record.get("avg_rtt_ms"),
            record.get("max_rtt_ms"),

            record.get("jitter_ms"),
            record.get("avg_jitter_ms"),

            record.get("packet_loss_pct"),

            record.get("samples"),
            record.get("successful"),
            record.get("failed"),

            record.get("disconnects"),
            record.get("reconnects"),
            record.get("reconnect_attempts"),

            record.get("bssid_changes"),
            record.get("channel_changes"),
            record.get("gateway_changes"),

            record.get("weak_signal_events"),
            record.get("high_latency_events"),

            bool_to_int(
                record.get(
                    "weak_signal_active"
                )
            ),

            bool_to_int(
                record.get(
                    "high_latency_active"
                )
            ),
        ),
    )


def insert_event(
    conn,
    record_id,
    record,
    received_at,
):
    details = record.get(
        "details",
        {},
    )

    conn.execute(
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
            record.get("device_id"),
            record.get("uptime_ms"),
            record.get("event"),
            record.get("severity"),
            json.dumps(
                details,
                separators=(",", ":"),
                ensure_ascii=False,
            ),
        ),
    )


def process_line(
    conn,
    line,
    verbose=False,
):
    line = line.strip()

    if not line:
        return

    if not line.startswith("{"):
        if verbose:
            print(
                f"[SERIAL] {line}"
            )

        return

    try:
        record = json.loads(
            line
        )

    except json.JSONDecodeError as exc:
        print(
            f"[WARN] Invalid JSON: {exc}",
            file=sys.stderr,
        )

        if verbose:
            print(
                f"[RAW] {line}",
                file=sys.stderr,
            )

        return

    if not isinstance(
        record,
        dict,
    ):
        return

    received_at = utc_now()

    insert_record(
        conn,
        record,
        received_at,
    )

    record_type = record.get(
        "type"
    )

    if record_type == "event":
        print(
            "[EVENT]",
            received_at,
            record.get(
                "severity",
                "unknown",
            ).upper(),
            record.get(
                "event",
                "UNKNOWN",
            ),
            record.get(
                "details",
                {},
            ),
        )

    elif (
        record_type ==
        "measurement"
        and verbose
    ):
        print(
            "[MEASUREMENT]",
            received_at,
            record.get(
                "device_id"
            ),
            record.get(
                "status"
            ),
            f"IP={record.get('ip_address')}",
            f"RTT={record.get('internet_rtt_ms')}",
            f"DNS={record.get('dns_latency_ms')}",
            f"RSSI={record.get('rssi_dbm')}",
        )


def run_collector(
    port,
    baud,
    database,
    verbose,
):
    conn = open_database(
        database
    )

    create_schema(
        conn
    )

    print(
        f"[DB] {database}"
    )

    while True:
        try:
            print(
                f"[SERIAL] Opening {port} "
                f"at {baud} baud..."
            )

            with serial.Serial(
                port=port,
                baudrate=baud,
                timeout=1,
            ) as serial_port:
                print(
                    f"[SERIAL] Connected to {port}"
                )

                while True:
                    try:
                        raw = (
                            serial_port
                            .readline()
                        )

                    except SerialException:
                        raise

                    if not raw:
                        continue

                    line = raw.decode(
                        "utf-8",
                        errors="replace",
                    )

                    process_line(
                        conn,
                        line,
                        verbose=verbose,
                    )

        except SerialException as exc:
            print(
                f"[SERIAL] Connection lost: {exc}"
            )

            print(
                "[SERIAL] Retrying in 2 seconds..."
            )

            time.sleep(2)

        except KeyboardInterrupt:
            print(
                "\nStopping collector."
            )

            break

        except Exception as exc:
            print(
                f"[ERROR] {exc}",
                file=sys.stderr,
            )

            print(
                "[SERIAL] Retrying in 2 seconds..."
            )

            time.sleep(2)

    conn.close()


def parse_args():
    parser = argparse.ArgumentParser(
        description=(
            "PicoNetANALyzer USB telemetry collector"
        )
    )

    parser.add_argument(
        "--port",
        default=DEFAULT_PORT,
        help=(
            "Serial device "
            f"(default: {DEFAULT_PORT})"
        ),
    )

    parser.add_argument(
        "--baud",
        type=int,
        default=DEFAULT_BAUD,
        help=(
            "Serial baud rate "
            f"(default: {DEFAULT_BAUD})"
        ),
    )

    parser.add_argument(
        "--db",
        default=str(DEFAULT_DB),
        help=(
            "SQLite database path "
            f"(default: {DEFAULT_DB})"
        ),
    )

    parser.add_argument(
        "--verbose",
        action="store_true",
        help="Print measurements as they arrive",
    )

    return parser.parse_args()


def main():
    args = parse_args()

    run_collector(
        port=args.port,
        baud=args.baud,
        database=args.db,
        verbose=args.verbose,
    )


if __name__ == "__main__":
    main()
