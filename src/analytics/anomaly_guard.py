"""
src/analytics/anomaly_guard.py — UC-10 Anomaly Guard

Real-time anomaly detection: flags features whose recent latency or event
count deviates more than N standard deviations from their 8-week baseline.
"""

from __future__ import annotations

import duckdb

from ._db import get_ro_connection, rows


def get_anomalies(
    conn: duckdb.DuckDBPyConnection | None = None,
    lookback_days: int = 3,
    z_threshold: float = 3.0,
    min_events: int = 5,
) -> dict:
    """
    Compare recent (last N days) avg_ms against the 8-week baseline.
    Flags features where (recent_avg - mean) / std > z_threshold.

    Parameters
    ----------
    lookback_days : int
        How many days constitute 'recent'.
    z_threshold : float
        Standard deviations above mean to flag as anomalous.
    min_events : int
        Minimum event count in the recent window.

    Returns
    -------
    dict with meta + data sorted by z_score desc
    """
    if conn is None:
        conn = get_ro_connection()

    cur = conn.execute("""
        WITH recent AS (
            SELECT
                feature,
                AVG(avg_ms)     AS recent_avg_ms,
                AVG(p95_ms)     AS recent_p95_ms,
                SUM(event_count) AS recent_events
            FROM feature_daily_stats
            WHERE stat_date >= CURRENT_DATE - INTERVAL (? || ' days')
            GROUP BY feature
            HAVING SUM(event_count) >= ?
        )
        SELECT
            fb.feature,
            fb.feature_type,
            fb.mean_avg_ms                                           AS baseline_avg_ms,
            fb.std_avg_ms                                            AS baseline_std_ms,
            fb.mean_p95_ms                                           AS baseline_p95_ms,
            r.recent_avg_ms,
            r.recent_p95_ms,
            r.recent_events,
            (r.recent_avg_ms - fb.mean_avg_ms) / NULLIF(fb.std_avg_ms, 0) AS z_score,
            (r.recent_avg_ms - fb.mean_avg_ms) / NULLIF(fb.mean_avg_ms, 0) * 100 AS delta_pct
        FROM feature_baselines fb
        JOIN recent r USING (feature)
        WHERE ABS((r.recent_avg_ms - fb.mean_avg_ms) / NULLIF(fb.std_avg_ms, 0)) >= ?
        ORDER BY z_score DESC
    """, [str(lookback_days), min_events, z_threshold])

    data = rows(cur)

    # Separate positive (degradation) from negative (unexpected improvement)
    degraded   = [r for r in data if (r.get("z_score") or 0) > 0]
    improved   = [r for r in data if (r.get("z_score") or 0) < 0]

    return {
        "meta": {
            "use_case": "UC-10",
            "title": "Anomaly Guard",
            "lookback_days": lookback_days,
            "z_threshold": z_threshold,
            "min_events": min_events,
            "total_anomalies": len(data),
            "degraded_count": len(degraded),
            "improved_count": len(improved),
        },
        "data": data,
    }


def get_count_anomalies(
    conn: duckdb.DuckDBPyConnection | None = None,
    lookback_days: int = 3,
    z_threshold: float = 3.0,
    min_events: int = 5,
) -> dict:
    """
    Same as get_anomalies but for event_count deviations.
    Spikes or drops in usage volume can indicate UI bugs or outages.
    """
    if conn is None:
        conn = get_ro_connection()

    cur = conn.execute("""
        WITH recent AS (
            SELECT
                feature,
                AVG(event_count)  AS recent_avg_count,
                SUM(event_count)  AS recent_total
            FROM feature_daily_stats
            WHERE stat_date >= CURRENT_DATE - INTERVAL (? || ' days')
            GROUP BY feature
            HAVING SUM(event_count) >= ?
        )
        SELECT
            fb.feature,
            fb.feature_type,
            fb.mean_daily_count                                              AS baseline_avg_count,
            fb.std_daily_count                                               AS baseline_std_count,
            r.recent_avg_count,
            r.recent_total,
            (r.recent_avg_count - fb.mean_daily_count) / NULLIF(fb.std_daily_count, 0) AS z_score,
            (r.recent_avg_count - fb.mean_daily_count) / NULLIF(fb.mean_daily_count, 0) * 100 AS delta_pct
        FROM feature_baselines fb
        JOIN recent r USING (feature)
        WHERE ABS((r.recent_avg_count - fb.mean_daily_count) / NULLIF(fb.std_daily_count, 0)) >= ?
        ORDER BY ABS(z_score) DESC
    """, [str(lookback_days), min_events, z_threshold])

    return {
        "meta": {
            "use_case": "UC-10",
            "title": "Usage Count Anomalies",
            "lookback_days": lookback_days,
            "z_threshold": z_threshold,
        },
        "data": rows(cur),
    }


def get_alert_summary(
    conn: duckdb.DuckDBPyConnection | None = None,
    lookback_days: int = 1,
    z_threshold: float = 2.5,
) -> dict:
    """
    Lightweight daily alert summary: count of latency and count anomalies.
    Suitable for a dashboard banner or Slack notification.
    """
    latency = get_anomalies(conn, lookback_days=lookback_days, z_threshold=z_threshold)
    counts  = get_count_anomalies(conn, lookback_days=lookback_days, z_threshold=z_threshold)

    return {
        "meta": {
            "use_case": "UC-10",
            "title": "Alert Summary",
            "lookback_days": lookback_days,
            "z_threshold": z_threshold,
        },
        "latency_anomalies": latency["data"],
        "count_anomalies":   counts["data"],
        "summary": {
            "latency_degraded":  latency["meta"]["degraded_count"],
            "latency_improved":  latency["meta"]["improved_count"],
            "count_anomalies":   len(counts["data"]),
        },
    }
