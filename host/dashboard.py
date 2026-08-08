#!/usr/bin/env python3

import csv
import io
import json
import os
import sqlite3

from pathlib import Path
from typing import Iterator

from fastapi import (
    FastAPI,
    HTTPException,
    Query,
)

from fastapi.responses import (
    FileResponse,
    StreamingResponse,
)

from fastapi.staticfiles import StaticFiles


HOST_DIR = (
    Path(__file__)
    .resolve()
    .parent
)

PROJECT_ROOT = (
    HOST_DIR.parent
)

STATIC_DIR = (
    HOST_DIR
    / "static"
)

DEFAULT_DB = (
    PROJECT_ROOT
    / "data"
    / "piconet.db"
)

DB_PATH = Path(
    os.environ.get(
        "PICONET_DB",
        str(DEFAULT_DB),
    )
)


app = FastAPI(
    title="PicoNetANALyzer Dashboard",
    version="1.0",
)


app.mount(
    "/static",
    StaticFiles(
        directory=STATIC_DIR
    ),
    name="static",
)


def open_database():
    if not DB_PATH.exists():
        raise HTTPException(
            status_code=503,
            detail=(
                f"Database does not exist: "
                f"{DB_PATH}"
            ),
        )

    conn = sqlite3.connect(
        DB_PATH,
        timeout=5,
    )

    conn.row_factory = (
        sqlite3.Row
    )

    conn.execute(
        "PRAGMA query_only=ON"
    )

    return conn


def row_to_dict(row):
    if row is None:
        return None

    return dict(row)


def resolve_device(
    conn,
    device_id=None,
):
    if device_id:
        return device_id

    row = conn.execute(
        """
        SELECT device_id
        FROM measurements
        WHERE device_id IS NOT NULL
        ORDER BY record_id DESC
        LIMIT 1
        """
    ).fetchone()

    if row is None:
        return None

    return row[
        "device_id"
    ]


@app.get("/")
def index():
    return FileResponse(
        STATIC_DIR
        / "dashboard.html"
    )


@app.get("/api/health")
def health():
    return {
        "status": "ok",
        "database": str(
            DB_PATH
        ),
        "database_exists":
            DB_PATH.exists(),
    }


@app.get("/api/devices")
def devices():
    conn = open_database()

    try:
        rows = conn.execute(
            """
            SELECT
                device_id,
                MAX(received_at)
                    AS last_seen
            FROM measurements
            WHERE device_id IS NOT NULL
            GROUP BY device_id
            ORDER BY last_seen DESC
            """
        ).fetchall()

        return {
            "devices": [
                dict(row)
                for row in rows
            ]
        }

    finally:
        conn.close()


@app.get("/api/status")
def status(
    device_id: str | None = None,
):
    conn = open_database()

    try:
        device = resolve_device(
            conn,
            device_id,
        )

        if device is None:
            raise HTTPException(
                status_code=404,
                detail=(
                    "No measurement data available."
                ),
            )

        row = conn.execute(
            """
            SELECT *
            FROM measurements
            WHERE device_id = ?
            ORDER BY record_id DESC
            LIMIT 1
            """,
            (
                device,
            ),
        ).fetchone()

        if row is None:
            raise HTTPException(
                status_code=404,
                detail=(
                    f"No data for device {device}"
                ),
            )

        return dict(row)

    finally:
        conn.close()


@app.get("/api/measurements")
def measurements(
    limit: int = Query(
        default=180,
        ge=1,
        le=5000,
    ),
    device_id: str | None = None,
):
    conn = open_database()

    try:
        device = resolve_device(
            conn,
            device_id,
        )

        if device is None:
            return {
                "device_id": None,
                "measurements": [],
            }

        rows = conn.execute(
            """
            SELECT *
            FROM measurements
            WHERE device_id = ?
            ORDER BY record_id DESC
            LIMIT ?
            """,
            (
                device,
                limit,
            ),
        ).fetchall()

        values = [
            dict(row)
            for row in reversed(rows)
        ]

        return {
            "device_id": device,
            "measurements": values,
        }

    finally:
        conn.close()


@app.get("/api/events")
def events(
    limit: int = Query(
        default=100,
        ge=1,
        le=5000,
    ),
    device_id: str | None = None,
    severity: str | None = None,
    event_type: str | None = None,
):
    conn = open_database()

    try:
        device = resolve_device(
            conn,
            device_id,
        )

        if device is None:
            return {
                "device_id": None,
                "events": [],
            }

        conditions = [
            "device_id = ?"
        ]

        parameters = [
            device
        ]

        if severity:
            conditions.append(
                "severity = ?"
            )

            parameters.append(
                severity
            )

        if event_type:
            conditions.append(
                "event_type = ?"
            )

            parameters.append(
                event_type
            )

        where_clause = (
            " AND ".join(
                conditions
            )
        )

        parameters.append(
            limit
        )

        rows = conn.execute(
            f"""
            SELECT *
            FROM events
            WHERE {where_clause}
            ORDER BY record_id DESC
            LIMIT ?
            """,
            parameters,
        ).fetchall()

        result = []

        for row in rows:
            item = dict(row)

            raw_details = item.pop(
                "details_json",
                None,
            )

            try:
                item["details"] = (
                    json.loads(
                        raw_details
                    )
                    if raw_details
                    else {}
                )

            except json.JSONDecodeError:
                item["details"] = {
                    "raw": raw_details
                }

            result.append(
                item
            )

        return {
            "device_id": device,
            "events": result,
        }

    finally:
        conn.close()


@app.get("/api/event-types")
def event_types(
    device_id: str | None = None,
):
    conn = open_database()

    try:
        device = resolve_device(
            conn,
            device_id,
        )

        if device is None:
            return {
                "event_types": []
            }

        rows = conn.execute(
            """
            SELECT DISTINCT event_type
            FROM events
            WHERE
                device_id = ?
                AND event_type IS NOT NULL
            ORDER BY event_type
            """,
            (
                device,
            ),
        ).fetchall()

        return {
            "event_types": [
                row["event_type"]
                for row in rows
            ]
        }

    finally:
        conn.close()


@app.get("/api/raw")
def raw_records(
    limit: int = Query(
        default=100,
        ge=1,
        le=5000,
    ),
    device_id: str | None = None,
):
    conn = open_database()

    try:
        device = resolve_device(
            conn,
            device_id,
        )

        if device is None:
            return {
                "records": []
            }

        rows = conn.execute(
            """
            SELECT *
            FROM records
            WHERE device_id = ?
            ORDER BY id DESC
            LIMIT ?
            """,
            (
                device,
                limit,
            ),
        ).fetchall()

        result = []

        for row in rows:
            item = dict(row)

            try:
                item["payload"] = (
                    json.loads(
                        item[
                            "payload_json"
                        ]
                    )
                )

            except json.JSONDecodeError:
                item["payload"] = None

            result.append(
                item
            )

        return {
            "device_id": device,
            "records": result,
        }

    finally:
        conn.close()


def csv_stream(
    query,
    parameters,
) -> Iterator[str]:
    conn = open_database()

    try:
        cursor = conn.execute(
            query,
            parameters,
        )

        column_names = [
            item[0]
            for item
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
                list(row)
            )

            yield buffer.getvalue()

            buffer.seek(0)
            buffer.truncate(0)

    finally:
        conn.close()


@app.get(
    "/api/export/measurements.csv"
)
def export_measurements(
    device_id: str | None = None,
):
    conn = open_database()

    try:
        device = resolve_device(
            conn,
            device_id,
        )

    finally:
        conn.close()

    if device is None:
        raise HTTPException(
            status_code=404,
            detail="No device data available.",
        )

    safe_device = (
        device
        .replace(
            "/",
            "_",
        )
        .replace(
            "\\",
            "_",
        )
    )

    headers = {
        "Content-Disposition":
            (
                "attachment; "
                f'filename="'
                f'{safe_device}_measurements.csv"'
            )
    }

    return StreamingResponse(
        csv_stream(
            """
            SELECT *
            FROM measurements
            WHERE device_id = ?
            ORDER BY record_id ASC
            """,
            (
                device,
            ),
        ),
        media_type="text/csv",
        headers=headers,
    )


@app.get(
    "/api/export/events.csv"
)
def export_events(
    device_id: str | None = None,
):
    conn = open_database()

    try:
        device = resolve_device(
            conn,
            device_id,
        )

    finally:
        conn.close()

    if device is None:
        raise HTTPException(
            status_code=404,
            detail="No device data available.",
        )

    safe_device = (
        device
        .replace(
            "/",
            "_",
        )
        .replace(
            "\\",
            "_",
        )
    )

    headers = {
        "Content-Disposition":
            (
                "attachment; "
                f'filename="'
                f'{safe_device}_events.csv"'
            )
    }

    return StreamingResponse(
        csv_stream(
            """
            SELECT *
            FROM events
            WHERE device_id = ?
            ORDER BY record_id ASC
            """,
            (
                device,
            ),
        ),
        media_type="text/csv",
        headers=headers,
    )
