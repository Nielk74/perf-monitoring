"""
src/analytics/silent_degrader.py — UC-01 Silent Degrader

Detects features whose p95 latency has been gradually worsening over the
last N weeks, even when no single day crosses an alert threshold.
Uses the pre-computed weekly regression slope from feature_baselines.
"""

from __future__ import annotations

import duckdb

from ._db import get_ro_connection, rows


def get_silent_degraders(
    conn: duckdb.DuckDBPyConnection | None = None,
    min_slope_pct: float = 1.0,       # at least +1 % per week
    min_weeks_data: int = 7,           # need at least 7 days in the window
    top_n: int = 20,
) -> dict:
    """
    Returns features whose average latency has a significant positive weekly
    regression slope over the last 8 weeks.

    Parameters
    ----------
    min_slope_pct : float
        Minimum weekly slope as % of mean (weekly_slope_pct) to flag a feature.
    min_weeks_data : int
        Minimum number of daily rows inside the 8-week window (baseline filter).
    top_n : int
        Maximum rows to return, ordered by slope descending.

    Returns
    -------
    dict with keys:
        meta  : query parameters + summary counts
        data  : list of dicts, one per degrading feature
    """
    if conn is None:
        conn = get_ro_connection()

    cur = conn.execute("""
        SELECT
            fb.feature,
            fb.feature_type,
            fb.mean_avg_ms,
            fb.std_avg_ms,
            fb.mean_p95_ms,
            fb.weekly_slope_ms,
            fb.weekly_slope_pct,
            fb.mean_daily_count,
            fb.baseline_date,
            -- recent p95 from last 7 days for "current" snapshot
            recent.p95_last7
        FROM feature_baselines fb
        LEFT JOIN (
            SELECT
                feature,
                AVG(p95_ms) AS p95_last7
            FROM feature_daily_stats
            WHERE stat_date >= CURRENT_DATE - INTERVAL '7 days'
            GROUP BY feature
        ) recent USING (feature)
        WHERE fb.weekly_slope_pct >= ?
        ORDER BY fb.weekly_slope_pct DESC
        LIMIT ?
    """, [min_slope_pct, top_n])

    data = rows(cur)

    return {
        "meta": {
            "use_case": "UC-01",
            "title": "Silent Degrader Detection",
            "min_slope_pct_per_week": min_slope_pct,
            "min_weeks_data": min_weeks_data,
            "top_n": top_n,
            "result_count": len(data),
        },
        "data": data,
    }


def get_feature_trend(
    feature: str,
    conn: duckdb.DuckDBPyConnection | None = None,
    days: int = 56,
) -> dict:
    """
    Daily p95 / avg_ms trend for a single feature (for sparkline / detail view).
    """
    if conn is None:
        conn = get_ro_connection()

    cur = conn.execute("""
        SELECT
            stat_date,
            event_count,
            p50_ms,
            p95_ms,
            p99_ms,
            avg_ms,
            distinct_users,
            distinct_workstations
        FROM feature_daily_stats
        WHERE feature = ?
          AND stat_date >= CURRENT_DATE - INTERVAL (? || ' days')
        ORDER BY stat_date
    """, [feature, str(days)])

    return {
        "meta": {
            "use_case": "UC-01",
            "feature": feature,
            "days": days,
        },
        "data": rows(cur),
    }
