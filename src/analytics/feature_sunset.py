"""
src/analytics/feature_sunset.py — UC-05 Feature Sunset

Identifies features with declining usage over time — candidates for removal
or archival. Uses event_count trends from feature_daily_stats.
"""

from __future__ import annotations

import duckdb

from ._db import get_ro_connection, rows


def get_sunset_candidates(
    conn: duckdb.DuckDBPyConnection | None = None,
    days: int = 90,
    min_peak_events: int = 10,
    decline_threshold_pct: float = 50.0,
    top_n: int = 30,
) -> dict:
    """
    Features whose recent event_count has dropped significantly vs their peak.

    Parameters
    ----------
    days : int
        Total window to consider.
    min_peak_events : int
        Minimum peak daily event count — filters out always-tiny features.
    decline_threshold_pct : float
        Minimum % decline from peak to recent avg to flag.
    top_n : int
        Max rows returned.

    Returns
    -------
    dict with meta + data (feature, peak_count, recent_avg, decline_pct, last_seen)
    """
    if conn is None:
        conn = get_ro_connection()

    half_days = days // 2

    cur = conn.execute("""
        WITH early AS (
            SELECT
                feature,
                feature_type,
                AVG(event_count) AS early_avg
            FROM feature_daily_stats
            WHERE stat_date >= CURRENT_DATE - INTERVAL (? || ' days')
              AND stat_date <  CURRENT_DATE - INTERVAL (? || ' days')
            GROUP BY feature, feature_type
        ),
        recent AS (
            SELECT
                feature,
                AVG(event_count) AS recent_avg,
                MAX(stat_date)   AS last_seen,
                MAX(event_count) AS peak_count
            FROM feature_daily_stats
            WHERE stat_date >= CURRENT_DATE - INTERVAL (? || ' days')
            GROUP BY feature
        )
        SELECT
            e.feature,
            e.feature_type,
            e.early_avg,
            r.recent_avg,
            r.peak_count,
            r.last_seen,
            (e.early_avg - r.recent_avg) / NULLIF(e.early_avg, 0) * 100 AS decline_pct
        FROM early e
        JOIN recent r USING (feature)
        WHERE r.peak_count >= ?
          AND (e.early_avg - r.recent_avg) / NULLIF(e.early_avg, 0) * 100 >= ?
        ORDER BY decline_pct DESC
        LIMIT ?
    """, [str(days), str(half_days), str(half_days), min_peak_events, decline_threshold_pct, top_n])

    data = rows(cur)

    return {
        "meta": {
            "use_case": "UC-05",
            "title": "Feature Sunset Candidates",
            "days": days,
            "half_days": half_days,
            "min_peak_events": min_peak_events,
            "decline_threshold_pct": decline_threshold_pct,
            "top_n": top_n,
            "result_count": len(data),
        },
        "data": data,
    }


def get_zombie_features(
    conn: duckdb.DuckDBPyConnection | None = None,
    inactive_days: int = 30,
    min_total_events: int = 5,
) -> dict:
    """
    Features not seen in the last N days but present before that.
    These are 'zombie' features — effectively dead but not removed.
    """
    if conn is None:
        conn = get_ro_connection()

    cur = conn.execute("""
        SELECT
            feature,
            feature_type,
            MAX(stat_date)      AS last_seen,
            SUM(event_count)    AS total_events,
            CURRENT_DATE - MAX(stat_date) AS days_silent
        FROM feature_daily_stats
        GROUP BY feature, feature_type
        HAVING MAX(stat_date) < CURRENT_DATE - INTERVAL (? || ' days')
           AND SUM(event_count) >= ?
        ORDER BY last_seen DESC
    """, [str(inactive_days), min_total_events])

    return {
        "meta": {
            "use_case": "UC-05",
            "title": "Zombie Features",
            "inactive_days": inactive_days,
            "min_total_events": min_total_events,
        },
        "data": rows(cur),
    }


def get_usage_trend(
    feature: str,
    conn: duckdb.DuckDBPyConnection | None = None,
    days: int = 90,
) -> dict:
    """Daily event_count trend for a single feature."""
    if conn is None:
        conn = get_ro_connection()

    cur = conn.execute("""
        SELECT
            stat_date,
            event_count,
            distinct_users,
            distinct_workstations
        FROM feature_daily_stats
        WHERE feature = ?
          AND stat_date >= CURRENT_DATE - INTERVAL (? || ' days')
        ORDER BY stat_date
    """, [feature, str(days)])

    return {
        "meta": {"use_case": "UC-05", "feature": feature, "days": days},
        "data": rows(cur),
    }
