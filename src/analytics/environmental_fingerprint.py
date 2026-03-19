"""
src/analytics/environmental_fingerprint.py — UC-03 Environmental Fingerprint

Compares latency distributions across offices (derived from workstation prefix)
to surface environment-specific bottlenecks invisible in global averages.
"""

from __future__ import annotations

import duckdb

from ._db import get_ro_connection, rows


# Map 3-char workstation prefix → office label
OFFICE_MAP = {
    "LON": "London",
    "NYC": "New York",
    "SIN": "Singapore",
}


def get_office_comparison(
    conn: duckdb.DuckDBPyConnection | None = None,
    days: int = 30,
    min_events: int = 20,
    feature_type: str | None = None,
) -> dict:
    """
    Per-office, per-feature avg and p95 latency over the last N days.

    Parameters
    ----------
    days : int
        Rolling window in days.
    min_events : int
        Minimum total events per office+feature to include.
    feature_type : str or None
        Optional filter (e.g. 'Trade Entry').

    Returns
    -------
    dict with meta + data (list of {office, feature, avg_ms, p95_ms, event_count, ...})
    """
    if conn is None:
        conn = get_ro_connection()

    type_filter = "AND feature_type = ?" if feature_type else ""
    params: list = [str(days)]
    if feature_type:
        params.append(feature_type)
    params.append(min_events)

    cur = conn.execute(f"""
        SELECT
            SUBSTR(workstation, 1, 3)                                   AS office_prefix,
            feature_type,
            feature,
            COUNT(*)                                                     AS event_count,
            AVG(duration_ms)  FILTER (WHERE duration_ms IS NOT NULL)     AS avg_ms,
            PERCENTILE_CONT(0.95) WITHIN GROUP (ORDER BY duration_ms)
                FILTER (WHERE duration_ms IS NOT NULL)                   AS p95_ms,
            PERCENTILE_CONT(0.50) WITHIN GROUP (ORDER BY duration_ms)
                FILTER (WHERE duration_ms IS NOT NULL)                   AS p50_ms
        FROM audit_events
        WHERE mod_dt >= CURRENT_TIMESTAMP - INTERVAL (? || ' days')
          AND supp_user = asmd_user
          {type_filter}
        GROUP BY SUBSTR(workstation, 1, 3), feature_type, feature
        HAVING COUNT(*) >= ?
        ORDER BY office_prefix, feature_type, feature
    """, params)

    data = rows(cur)

    # Pivot to global avg for delta calculation
    global_cur = conn.execute(f"""
        SELECT
            feature,
            AVG(duration_ms) FILTER (WHERE duration_ms IS NOT NULL) AS global_avg_ms
        FROM audit_events
        WHERE mod_dt >= CURRENT_TIMESTAMP - INTERVAL (? || ' days')
          AND supp_user = asmd_user
          {type_filter}
        GROUP BY feature
    """, params[:1] + (params[1:-1] if feature_type else []))

    global_avg = {r["feature"]: r["global_avg_ms"] for r in rows(global_cur)}

    for row in data:
        g = global_avg.get(row["feature"])
        if g and row["avg_ms"] is not None:
            row["delta_vs_global_pct"] = (row["avg_ms"] - g) / g * 100
        else:
            row["delta_vs_global_pct"] = None

    return {
        "meta": {
            "use_case": "UC-03",
            "title": "Environmental Fingerprint",
            "days": days,
            "min_events": min_events,
            "feature_type": feature_type,
            "result_count": len(data),
        },
        "data": data,
    }


def get_workstation_outliers(
    conn: duckdb.DuckDBPyConnection | None = None,
    days: int = 30,
    min_events: int = 50,
    top_n: int = 20,
) -> dict:
    """
    Returns workstations with the highest avg startup latency vs office peers.
    """
    if conn is None:
        conn = get_ro_connection()

    cur = conn.execute("""
        SELECT
            workstation,
            office_prefix,
            last_seen,
            total_events,
            avg_startup_ms,
            p95_startup_ms
        FROM workstation_profiles
        WHERE avg_startup_ms IS NOT NULL
          AND total_events >= ?
        ORDER BY avg_startup_ms DESC
        LIMIT ?
    """, [min_events, top_n])

    return {
        "meta": {
            "use_case": "UC-03",
            "title": "Workstation Startup Outliers",
            "days": days,
            "min_events": min_events,
            "top_n": top_n,
        },
        "data": rows(cur),
    }
