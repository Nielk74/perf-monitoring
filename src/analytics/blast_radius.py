"""
src/analytics/blast_radius.py — UC-02 Blast Radius

Given a deployment date (or the nearest tagged commit), shows which features
experienced a statistically significant latency change in the N days after
vs the N days before.
"""

from __future__ import annotations

import duckdb

from ._db import get_ro_connection, rows


def get_blast_radius(
    deployed_at: str,
    conn: duckdb.DuckDBPyConnection | None = None,
    window_days: int = 14,
    min_events: int = 10,
    z_threshold: float = 2.0,
) -> dict:
    """
    Compare per-feature avg_ms before vs after a deployment date.

    Parameters
    ----------
    deployed_at : str
        ISO date string (YYYY-MM-DD) of the deployment.
    window_days : int
        Days of pre/post window to compare.
    min_events : int
        Minimum event count in both windows to include a feature.
    z_threshold : float
        Minimum z-score (change / std_daily_count of baseline) to flag.

    Returns
    -------
    dict with keys:
        meta  : parameters + nearby commit info
        data  : per-feature before/after stats, sorted by abs delta desc
    """
    if conn is None:
        conn = get_ro_connection()

    # Find the nearest commit with deployed_at set
    commit_cur = conn.execute("""
        SELECT commit_hash, tag, committed_at, deployed_at
        FROM commits
        WHERE deployed_at IS NOT NULL
          AND ABS(epoch(CAST(deployed_at AS TIMESTAMPTZ)) - epoch(CAST(? AS TIMESTAMPTZ))) < 86400 * 3
        ORDER BY ABS(epoch(CAST(deployed_at AS TIMESTAMPTZ)) - epoch(CAST(? AS TIMESTAMPTZ)))
        LIMIT 1
    """, [deployed_at, deployed_at])
    commit_row = commit_cur.fetchone()
    nearest_commit = (
        dict(zip([d[0] for d in commit_cur.description], commit_row))
        if commit_row
        else None
    )

    cur = conn.execute("""
        WITH before AS (
            SELECT
                feature,
                feature_type,
                COUNT(*)        AS event_count,
                AVG(avg_ms)     AS avg_ms,
                AVG(p95_ms)     AS p95_ms,
                STDDEV(avg_ms)  AS std_ms
            FROM feature_daily_stats
            WHERE stat_date >= CAST(? AS DATE) - INTERVAL (? || ' days')
              AND stat_date <  CAST(? AS DATE)
            GROUP BY feature, feature_type
            HAVING COUNT(*) >= ?
        ),
        after AS (
            SELECT
                feature,
                COUNT(*)        AS event_count,
                AVG(avg_ms)     AS avg_ms,
                AVG(p95_ms)     AS p95_ms
            FROM feature_daily_stats
            WHERE stat_date >= CAST(? AS DATE)
              AND stat_date <  CAST(? AS DATE) + INTERVAL (? || ' days')
            GROUP BY feature
            HAVING COUNT(*) >= ?
        )
        SELECT
            b.feature,
            b.feature_type,
            b.avg_ms                                                 AS before_avg_ms,
            a.avg_ms                                                 AS after_avg_ms,
            b.p95_ms                                                 AS before_p95_ms,
            a.p95_ms                                                 AS after_p95_ms,
            a.avg_ms - b.avg_ms                                      AS delta_avg_ms,
            b.event_count                                            AS before_days,
            a.event_count                                            AS after_days,
            (a.avg_ms - b.avg_ms) / NULLIF(b.std_ms, 0)             AS z_score,
            (a.avg_ms - b.avg_ms) / NULLIF(b.avg_ms, 0) * 100       AS delta_pct
        FROM before b
        JOIN after a USING (feature)
        WHERE ABS((a.avg_ms - b.avg_ms) / NULLIF(b.std_ms, 0)) >= ?
        ORDER BY ABS(a.avg_ms - b.avg_ms) DESC
    """, [
        deployed_at, str(window_days), deployed_at,
        min_events,
        deployed_at, deployed_at, str(window_days),
        min_events,
        z_threshold,
    ])

    data = rows(cur)

    return {
        "meta": {
            "use_case": "UC-02",
            "title": "Blast Radius Analysis",
            "deployed_at": deployed_at,
            "window_days": window_days,
            "min_events": min_events,
            "z_threshold": z_threshold,
            "nearest_commit": nearest_commit,
            "result_count": len(data),
        },
        "data": data,
    }


def list_deployments(
    conn: duckdb.DuckDBPyConnection | None = None,
    limit: int = 50,
) -> dict:
    """List all tagged deployments available for blast radius analysis."""
    if conn is None:
        conn = get_ro_connection()

    cur = conn.execute("""
        SELECT
            tag,
            repo_name,
            committed_at,
            deployed_at,
            author_name,
            message
        FROM commits
        WHERE deployed_at IS NOT NULL
        ORDER BY deployed_at DESC
        LIMIT ?
    """, [limit])

    return {
        "meta": {"use_case": "UC-02", "title": "Deployment List"},
        "data": rows(cur),
    }
