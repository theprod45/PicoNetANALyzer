#!/usr/bin/env python3

import csv
import io
import json
import os
import sqlite3

from pathlib import Path
from typing import Generator

from fastapi import FastAPI, HTTPException, Query
from fastapi.responses import FileResponse, StreamingResponse
from fastapi.staticfiles import StaticFiles


# ---------------------------------------------------------------------------
# Paths
# ---------------------------------------------------------------------------

HOST_DIR = Path(__file__).resolve().parent
PROJECT_ROOT = HOST_DIR.parent
STATIC_DIR = HOST_DIR / "static"

DEFAULT_DB_PATH = PROJECT_ROOT / "data" / "piconet.db"

DB_PATH = Path(
    os.environ.get(
        "PICONET_DB",
        str(DEFAULT_DB_PATH)
    )
)


# ---------------------------------------------------------------------------
# FastAPI
# ---------------------------------------------------------------------------

app = FastAPI(
    title="PicoNetANALyzer Dashboard",
    description=(
        "Live monitoring and diagnostics API for "
        "PicoNetANALyzer."
    ),
    version="0.1.0"
)


app.mount(
    "/static",
    StaticFiles(
        directory=str(STATIC_DIR)
    ),
    name="static"
)


# ---------------------------------------------------------------------------
# Database
# ---------------------------------------------------------------------------

def database_exists() -> bool:
    return DB_PATH.exists()


def open_database() -> sqlite3.Connection:
    """
    Open SQLite in read-only/query mode.

    collector.py is responsible for writing.
    dashboard.py only reads.
    """

    if not database_exists():
        raise HTTPException(
            status_code=503,
            detail=(
                f"Database does not exist: {DB_PATH}. "
                "Start collector.py first."
            )
        )

    connection = sqlite3.connect(
        DB_PATH,
        timeout=5
    )

    connection.row_factory = sqlite3.Row

    connection.execute(
        "PRAGMA query_only=ON"
    )

    return connection


def row_to_dict(
    row: sqlite3.Row
) -> dict:
    return dict(row)


def resolve_device(
    connection: sqlite3.Connection,
    requested_device: str | None
) -> str | None:
    """
    If a device was explicitly requested, use it.

    Otherwise select the device that most recently
    submitted a measurement.
    """

    if requested_device:
        return requested_device

    row = connection.execute(
        """
        SELECT device_id
        FROM measurements
        ORDER BY record_id DESC
        LIMIT 1
        """
    ).fetchone()

    if row is None:
        return None

    return row["device_id"]


# ---------------------------------------------------------------------------
# Frontend
# ---------------------------------------------------------------------------

@app.get("/")
def dashboard():
    """
    Serve the browser dashboard.
    """

    dashboard_file = (
        STATIC_DIR /
        "dashboard.html"
    )

    if not dashboard_file.exists():
        raise HTTPException(
            status_code=500,
            detail=(
                "dashboard.html was not found"
            )
        )

    return FileResponse(
        dashboard_file
    )


# ---------------------------------------------------------------------------
# Health
# ---------------------------------------------------------------------------

@app.get("/api/health")
def health():
    """
    Basic dashboard/database health information.
    """

    return {
        "status": "ok",
        "database": str(DB_PATH),
        "database_exists": database_exists()
    }


# ---------------------------------------------------------------------------
# Devices
# ---------------------------------------------------------------------------

@app.get("/api/devices")
def get_devices():
    """
    List every Pico device that has submitted
    measurements.

    This makes the dashboard ready for multiple
    PicoNetANALyzer devices later.
    """

    connection = open_database()

    try:

        rows = connection.execute(
            """
            SELECT
                device_id,
                MAX(received_at) AS last_seen,
                COUNT(*) AS measurement_count
            FROM measurements
            GROUP BY device_id
            ORDER BY last_seen DESC
            """
        ).fetchall()

        return {
            "devices": [
                row_to_dict(row)
                for row in rows
            ]
        }

    finally:

        connection.close()


# ---------------------------------------------------------------------------
# Current status
# ---------------------------------------------------------------------------

@app.get("/api/status")
def get_status(
    device_id: str | None = None
):
    """
    Return the newest measurement for one device.
    """

    connection = open_database()

    try:

        device = resolve_device(
            connection,
            device_id
        )

        if device is None:
            return {
                "available": False,
                "device_id": None,
                "measurement": None
            }


        row = connection.execute(
            """
            SELECT *
            FROM measurements
            WHERE device_id = ?
            ORDER BY record_id DESC
            LIMIT 1
            """,
            (device,)
        ).fetchone()


        if row is None:
            return {
                "available": False,
                "device_id": device,
                "measurement": None
            }


        return {
            "available": True,
            "device_id": device,
            "measurement": row_to_dict(row)
        }

    finally:

        connection.close()


# ---------------------------------------------------------------------------
# Measurement history
# ---------------------------------------------------------------------------

@app.get("/api/measurements")
def get_measurements(
    limit: int = Query(
        default=300,
        ge=1,
        le=5000
    ),
    device_id: str | None = None
):
    """
    Return recent measurements.

    Results are returned oldest -> newest so
    graphing libraries can use them directly.
    """

    connection = open_database()

    try:

        device = resolve_device(
            connection,
            device_id
        )

        if device is None:
            return {
                "device_id": None,
                "measurements": []
            }


        rows = connection.execute(
            """
            SELECT *
            FROM measurements
            WHERE device_id = ?
            ORDER BY record_id DESC
            LIMIT ?
            """,
            (
                device,
                limit
            )
        ).fetchall()


        measurements = [
            row_to_dict(row)
            for row in reversed(rows)
        ]


        return {
            "device_id": device,
            "measurements": measurements
        }

    finally:

        connection.close()


# ---------------------------------------------------------------------------
# Event history
# ---------------------------------------------------------------------------

@app.get("/api/events")
def get_events(
    limit: int = Query(
        default=100,
        ge=1,
        le=5000
    ),
    device_id: str | None = None,
    severity: str | None = None,
    event_type: str | None = None
):
    """
    Return recent network/security events.

    Optional filters:

        severity=warning
        event_type=BSSID_CHANGED
    """

    connection = open_database()

    try:

        device = resolve_device(
            connection,
            device_id
        )

        if device is None:
            return {
                "device_id": None,
                "events": []
            }


        query = """
            SELECT *
            FROM events
            WHERE device_id = ?
        """

        parameters = [device]


        if severity:

            query += """
                AND severity = ?
            """

            parameters.append(
                severity
            )


        if event_type:

            query += """
                AND event_type = ?
            """

            parameters.append(
                event_type
            )


        query += """
            ORDER BY record_id DESC
            LIMIT ?
        """

        parameters.append(
            limit
        )


        rows = connection.execute(
            query,
            parameters
        ).fetchall()


        events = []


        for row in rows:

            event = row_to_dict(row)

            details_json = (
                event.get(
                    "details_json"
                )
            )


            try:

                event["details"] = (
                    json.loads(
                        details_json
                    )
                    if details_json
                    else {}
                )

            except json.JSONDecodeError:

                event["details"] = {
                    "raw": details_json
                }


            events.append(event)


        return {
            "device_id": device,
            "events": events
        }

    finally:

        connection.close()


# ---------------------------------------------------------------------------
# Event types
# ---------------------------------------------------------------------------

@app.get("/api/event-types")
def get_event_types(
    device_id: str | None = None
):
    """
    Return known event types.

    Useful for dashboard filters.
    """

    connection = open_database()

    try:

        device = resolve_device(
            connection,
            device_id
        )

        if device is None:
            return {
                "event_types": []
            }


        rows = connection.execute(
            """
            SELECT
                event_type,
                COUNT(*) AS count
            FROM events
            WHERE device_id = ?
            GROUP BY event_type
            ORDER BY event_type
            """,
            (device,)
        ).fetchall()


        return {
            "device_id": device,
            "event_types": [
                row_to_dict(row)
                for row in rows
            ]
        }

    finally:

        connection.close()


# ---------------------------------------------------------------------------
# CSV streaming
# ---------------------------------------------------------------------------

def generate_csv(
    query: str,
    parameters: tuple
) -> Generator[str, None, None]:
    """
    Stream SQLite query results as CSV.

    Streaming prevents a huge future database from
    having to be loaded entirely into RAM.
    """

    connection = open_database()

    try:

        cursor = connection.execute(
            query,
            parameters
        )


        column_names = [
            description[0]
            for description
            in cursor.description
        ]


        buffer = io.StringIO()

        writer = csv.writer(
            buffer
        )


        writer.writerow(
            column_names
        )


        yield buffer.getvalue()


        buffer.seek(0)
        buffer.truncate(0)


        for row in cursor:

            writer.writerow(
                [
                    row[column]
                    for column
                    in column_names
                ]
            )

            yield buffer.getvalue()

            buffer.seek(0)
            buffer.truncate(0)


    finally:

        connection.close()


# ---------------------------------------------------------------------------
# Measurement CSV export
# ---------------------------------------------------------------------------

@app.get("/api/export/measurements.csv")
def export_measurements(
    device_id: str | None = None
):
    connection = open_database()

    try:

        device = resolve_device(
            connection,
            device_id
        )

    finally:

        connection.close()


    if device is None:
        raise HTTPException(
            status_code=404,
            detail="No measurement data available"
        )


    query = """
        SELECT *
        FROM measurements
        WHERE device_id = ?
        ORDER BY record_id ASC
    """


    headers = {
        "Content-Disposition":
            (
                "attachment; "
                f'filename="'
                f'{device}_measurements.csv"'
            )
    }


    return StreamingResponse(
        generate_csv(
            query,
            (device,)
        ),
        media_type="text/csv",
        headers=headers
    )


# ---------------------------------------------------------------------------
# Event CSV export
# ---------------------------------------------------------------------------

@app.get("/api/export/events.csv")
def export_events(
    device_id: str | None = None
):
    connection = open_database()

    try:

        device = resolve_device(
            connection,
            device_id
        )

    finally:

        connection.close()


    if device is None:
        raise HTTPException(
            status_code=404,
            detail="No event data available"
        )


    query = """
        SELECT *
        FROM events
        WHERE device_id = ?
        ORDER BY record_id ASC
    """


    headers = {
        "Content-Disposition":
            (
                "attachment; "
                f'filename="'
                f'{device}_events.csv"'
            )
    }


    return StreamingResponse(
        generate_csv(
            query,
            (device,)
        ),
        media_type="text/csv",
        headers=headers
    )


# ---------------------------------------------------------------------------
# Raw records
# ---------------------------------------------------------------------------

@app.get("/api/raw")
def get_raw_records(
    limit: int = Query(
        default=100,
        ge=1,
        le=1000
    ),
    device_id: str | None = None
):
    """
    Debug/development endpoint.

    Lets you inspect the complete original JSON
    records preserved by collector.py.
    """

    connection = open_database()

    try:

        device = resolve_device(
            connection,
            device_id
        )

        if device is None:
            return {
                "records": []
            }


        rows = connection.execute(
            """
            SELECT *
            FROM records
            WHERE device_id = ?
            ORDER BY id DESC
            LIMIT ?
            """,
            (
                device,
                limit
            )
        ).fetchall()


        records = []


        for row in rows:

            record = row_to_dict(row)


            try:

                record["payload"] = (
                    json.loads(
                        record[
                            "payload_json"
                        ]
                    )
                )

            except json.JSONDecodeError:

                record["payload"] = None


            records.append(
                record
            )


        return {
            "device_id": device,
            "records": records
        }

    finally:

        connection.close()
