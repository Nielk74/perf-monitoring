"""
src/analytics/peak_pressure.py — UC-09 Peak Pressure

Identifies when the system is under peak load: high concurrent users,
high event rate, or latency spikes. Broken down by time-of-day, day-of-week,
and office timezone.
"""

from __future__ import annotations

import duckdb

from ._db import get_ro_connection, rows


def get_peak_hours(
    conn: duckdb.DuckDBPyConnection | None = None,
    days: int = 30,
    office_prefix: str | None = None,
) -> dict:
    """
    Average event volume and latency by hour-of-day (UTC) and day-of-week.

    Parameters
    ----------
    days : int
        Rolling window.
    office_prefix : str or None
        Restrict to workstations starting with this prefix (e.g. 'LON').
    """
    if conn is None:
        conn = get_ro_connection()

    office_filter = "AND SUBSTR(workstation, 1, 3) = ?" if office_prefix else ""
    params: list = [str(days)]
    if office_prefix:
        params.append(office_prefix)

    cur = conn.execute(f"""
        SELECT
            EXTRACT(DOW  FROM mod_dt)                                    AS day_of_week,
            EXTRACT(HOUR FROM mod_dt)                                    AS hour_utc,
            COUNT(*)                                                     AS event_count,
            COUNT(DISTINCT asmd_user)                                    AS distinct_users,
            AVG(duration_ms) FILTER (WHERE duration_ms IS NOT NULL)      AS avg_ms,
            PERCENTILE_CONT(0.95) WITHIN GROUP (ORDER BY duration_ms)
                FILTER (WHERE duration_ms IS NOT NULL)                   AS p95_ms
        FROM audit_events
        WHERE mod_dt >= CURRENT_TIMESTAMP - INTERVAL (? || ' days')
          AND supp_user = asmd_user
          {office_filter}
        GROUP BY EXTRACT(DOW FROM mod_dt), EXTRACT(HOUR FROM mod_dt)
        ORDER BY day_of_week, hour_utc
    """, params)

    return {
        "meta": {
            "use_case": "UC-09",
            "title": "Peak Hours Heatmap",
            "days": days,
            "office_prefix": office_prefix,
        },
        "data": rows(cur),
    }


def get_daily_pressure(
    conn: duckdb.DuckDBPyConnection | None = None,
    days: int = 90,
) -> dict:
    """
    Daily event count, distinct users, and p95 latency — useful for spotting
    pressure spikes correlated with deployments or external events.
    """
    if conn is None:
        conn = get_ro_connection()

    cur = conn.execute("""
        SELECT
            stat_date,
            SUM(event_count)                    AS total_events,
            MAX(distinct_users)                 AS peak_daily_users,
            AVG(p95_ms)                         AS avg_p95_ms,
            MAX(p99_ms)                         AS max_p99_ms,
            SUM(total_wait_ms) / 1000.0 / 3600  AS total_wait_hours
        FROM feature_daily_stats
        WHERE stat_date >= CURRENT_DATE - INTERVAL (? || ' days')
        GROUP BY stat_date
        ORDER BY stat_date
    """, [str(days)])

    return {
        "meta": {
            "use_case": "UC-09",
            "title": "Daily Pressure",
            "days": days,
        },
        "data": rows(cur),
    }


def get_concurrent_users(
    conn: duckdb.DuckDBPyConnection | None = None,
    days: int = 30,
    bucket_minutes: int = 15,
) -> dict:
    """
    Approximate concurrent-user count in N-minute buckets.
    Uses session_start / session_end overlap.
    """
    if conn is None:
        conn = get_ro_connection()

    cur = conn.execute("""
        SELECT
            TIME_BUCKET(INTERVAL (? || ' minutes'), session_start) AS bucket,
            COUNT(*)                                               AS active_sessions,
            COUNT(DISTINCT asmd_user)                              AS distinct_users
        FROM user_sessions
        WHERE session_start >= CURRENT_TIMESTAMP - INTERVAL (? || ' days')
          AND is_impersonated = FALSE
        GROUP BY TIME_BUCKET(INTERVAL (? || ' minutes'), session_start)
        ORDER BY bucket
    """, [str(bucket_minutes), str(days), str(bucket_minutes)])

    return {
        "meta": {
            "use_case": "UC-09",
            "title": "Concurrent Users",
            "days": days,
            "bucket_minutes": bucket_minutes,
        },
        "data": rows(cur),
    }


def get_office_peak_overlap(
    conn: duckdb.DuckDBPyConnection | None = None,
    days: int = 30,
) -> dict:
    """
    Event counts per hour-of-day broken down by office prefix — reveals when
    multiple offices are simultaneously active (shared-infrastructure peak).
    """
    if conn is None:
        conn = get_ro_connection()

    cur = conn.execute("""
        SELECT
            EXTRACT(HOUR FROM mod_dt)           AS hour_utc,
            SUBSTR(workstation, 1, 3)           AS office_prefix,
            COUNT(*)                            AS event_count,
            COUNT(DISTINCT asmd_user)           AS distinct_users
        FROM audit_events
        WHERE mod_dt >= CURRENT_TIMESTAMP - INTERVAL (? || ' days')
          AND supp_user = asmd_user
        GROUP BY EXTRACT(HOUR FROM mod_dt), SUBSTR(workstation, 1, 3)
        ORDER BY hour_utc, office_prefix
    """, [str(days)])

    return {
        "meta": {
            "use_case": "UC-09",
            "title": "Office Peak Overlap",
            "days": days,
        },
        "data": rows(cur),
    }
