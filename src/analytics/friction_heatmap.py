"""
src/analytics/friction_heatmap.py — UC-06 Friction Heatmap

Ranks features by their aggregate user friction: high latency × high usage.
Surfaces the worst bottlenecks in terms of total wait time across all users.
"""

from __future__ import annotations

import duckdb

from ._db import get_ro_connection, rows


def get_friction_heatmap(
    conn: duckdb.DuckDBPyConnection | None = None,
    days: int = 30,
    min_events: int = 10,
    top_n: int = 40,
) -> dict:
    """
    Per-feature friction score = total_wait_ms (sum across all users in window).

    Parameters
    ----------
    days : int
        Rolling window in days.
    min_events : int
        Minimum event count in the window to include.
    top_n : int
        Max features returned.

    Returns
    -------
    dict with meta + data sorted by total_wait_ms desc
    """
    if conn is None:
        conn = get_ro_connection()

    cur = conn.execute("""
        SELECT
            feature,
            feature_type,
            SUM(event_count)                    AS total_events,
            SUM(total_wait_ms)                  AS total_wait_ms,
            SUM(total_wait_ms) / 1000.0 / 3600  AS total_wait_hours,
            AVG(avg_ms)                         AS avg_ms,
            AVG(p95_ms)                         AS p95_ms,
            AVG(distinct_users)                 AS avg_daily_users,
            SUM(distinct_users)                 AS total_user_days
        FROM feature_daily_stats
        WHERE stat_date >= CURRENT_DATE - INTERVAL (? || ' days')
        GROUP BY feature, feature_type
        HAVING SUM(event_count) >= ?
        ORDER BY total_wait_ms DESC
        LIMIT ?
    """, [str(days), min_events, top_n])

    data = rows(cur)

    # Compute friction score: normalise total_wait_hours to [0, 1]
    if data:
        max_wait = max((r["total_wait_hours"] or 0) for r in data)
        for row in data:
            w = row["total_wait_hours"] or 0
            row["friction_score"] = round(w / max_wait, 4) if max_wait else 0

    return {
        "meta": {
            "use_case": "UC-06",
            "title": "Friction Heatmap",
            "days": days,
            "min_events": min_events,
            "top_n": top_n,
            "result_count": len(data),
        },
        "data": data,
    }


def get_hourly_friction(
    conn: duckdb.DuckDBPyConnection | None = None,
    days: int = 30,
    feature_type: str | None = None,
) -> dict:
    """
    Average event volume and latency by hour-of-day (UTC) across the window.
    Useful for identifying peak-pressure windows.
    """
    if conn is None:
        conn = get_ro_connection()

    type_filter = "AND feature_type = ?" if feature_type else ""
    params: list = [str(days)]
    if feature_type:
        params.append(feature_type)

    cur = conn.execute(f"""
        SELECT
            EXTRACT(HOUR FROM mod_dt)                                    AS hour_utc,
            COUNT(*)                                                     AS event_count,
            AVG(duration_ms) FILTER (WHERE duration_ms IS NOT NULL)      AS avg_ms,
            PERCENTILE_CONT(0.95) WITHIN GROUP (ORDER BY duration_ms)
                FILTER (WHERE duration_ms IS NOT NULL)                   AS p95_ms,
            COUNT(DISTINCT asmd_user)                                    AS distinct_users
        FROM audit_events
        WHERE mod_dt >= CURRENT_TIMESTAMP - INTERVAL (? || ' days')
          AND supp_user = asmd_user
          {type_filter}
        GROUP BY EXTRACT(HOUR FROM mod_dt)
        ORDER BY hour_utc
    """, params)

    return {
        "meta": {
            "use_case": "UC-06",
            "title": "Hourly Friction Distribution",
            "days": days,
            "feature_type": feature_type,
        },
        "data": rows(cur),
    }


def get_user_friction(
    conn: duckdb.DuckDBPyConnection | None = None,
    days: int = 30,
    top_n: int = 20,
) -> dict:
    """
    Users ranked by total time spent waiting (total_wait_ms across all features).
    """
    if conn is None:
        conn = get_ro_connection()

    cur = conn.execute("""
        SELECT
            asmd_user,
            COUNT(*)                                                    AS event_count,
            SUM(duration_ms) FILTER (WHERE duration_ms IS NOT NULL)    AS total_wait_ms,
            AVG(duration_ms) FILTER (WHERE duration_ms IS NOT NULL)    AS avg_ms,
            COUNT(DISTINCT feature)                                     AS distinct_features,
            COUNT(DISTINCT workstation)                                 AS distinct_workstations
        FROM audit_events
        WHERE mod_dt >= CURRENT_TIMESTAMP - INTERVAL (? || ' days')
          AND supp_user = asmd_user
        GROUP BY asmd_user
        ORDER BY total_wait_ms DESC
        LIMIT ?
    """, [str(days), top_n])

    return {
        "meta": {
            "use_case": "UC-06",
            "title": "User Friction Ranking",
            "days": days,
            "top_n": top_n,
        },
        "data": rows(cur),
    }
