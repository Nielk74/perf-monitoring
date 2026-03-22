"""
src/analytics/blast_radius.py — UC-02 Blast Radius

Given a commit hash, shows which features experienced a statistically
significant latency change in the N days after the commit's nightly
deployment vs the N days before.

Deployment heuristic: every commit deploys the night it is made, so
deployed_at = committed_at + 1 day.
"""

from __future__ import annotations

import duckdb

from ._db import get_ro_connection, rows


def list_deployments(
    conn: duckdb.DuckDBPyConnection | None = None,
    limit: int = 180,
) -> dict:
    """
    List deployment nights grouped by DATE(deployed_at).
    Each row represents one nightly batch: all commits made on day D,
    deployed overnight and live on day D+1.
    """
    if conn is None:
        conn = get_ro_connection()

    cur = conn.execute("""
        SELECT
            CAST(c.deployed_at AS DATE)             AS deployed_date,
            COUNT(DISTINCT c.commit_hash)           AS commit_count,
            STRING_AGG(DISTINCT c.author_name, ', '
                ORDER BY c.author_name)             AS authors,
            COUNT(DISTINCT cf.file_path)            AS files_changed,
            COALESCE(SUM(cf.lines_added),    0)     AS lines_added,
            COALESCE(SUM(cf.lines_removed),  0)     AS lines_removed
        FROM commits c
        LEFT JOIN commit_files cf ON cf.commit_hash = c.commit_hash
        WHERE c.deployed_at IS NOT NULL
        GROUP BY CAST(c.deployed_at AS DATE)
        ORDER BY deployed_date DESC
        LIMIT ?
    """, [limit])

    return {
        "meta": {"use_case": "UC-02", "title": "Deployment List"},
        "data": rows(cur),
    }


def get_blast_radius(
    deployed_date: str,
    conn: duckdb.DuckDBPyConnection | None = None,
    window_days: int = 14,
    min_events: int = 10,
    z_threshold: float = 2.0,
) -> dict:
    """
    Compare per-feature avg_ms before vs after a deployment night.

    Parameters
    ----------
    deployed_date : str
        ISO date (YYYY-MM-DD) of the nightly deployment (= committed_at + 1 day).
    window_days : int
        Days of pre/post window to compare.
    min_events : int
        Minimum daily-stat rows in both windows to include a feature.
    z_threshold : float
        Minimum absolute z-score to flag a feature.
    """
    if conn is None:
        conn = get_ro_connection()

    # Summarise all commits in this deployment batch
    batch_cur = conn.execute("""
        SELECT
            COUNT(*)                                AS commit_count,
            STRING_AGG(DISTINCT author_name, ', '
                ORDER BY author_name)               AS authors,
            STRING_AGG(message, ' | '
                ORDER BY committed_at)              AS messages
        FROM commits
        WHERE CAST(deployed_at AS DATE) = CAST(? AS DATE)
    """, [deployed_date])
    batch_row = batch_cur.fetchone()
    batch_info = dict(zip([d[0] for d in batch_cur.description], batch_row)) if batch_row else {}

    deployed_at = deployed_date

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
            b.avg_ms                                           AS before_avg_ms,
            a.avg_ms                                           AS after_avg_ms,
            b.p95_ms                                           AS before_p95_ms,
            a.p95_ms                                           AS after_p95_ms,
            a.avg_ms - b.avg_ms                                AS delta_avg_ms,
            b.event_count                                      AS before_days,
            a.event_count                                      AS after_days,
            (a.avg_ms - b.avg_ms) / NULLIF(b.std_ms, 0)       AS z_score,
            (a.avg_ms - b.avg_ms) / NULLIF(b.avg_ms, 0) * 100 AS delta_pct
        FROM before b
        JOIN after a USING (feature)
        WHERE ABS((a.avg_ms - b.avg_ms) / NULLIF(b.std_ms, 0)) >= ?
        ORDER BY ABS(a.avg_ms - b.avg_ms) DESC
    """, [
        deployed_at, str(window_days), deployed_at, min_events,
        deployed_at, deployed_at, str(window_days), min_events,
        z_threshold,
    ])

    data = rows(cur)

    return {
        "meta": {
            "use_case": "UC-02",
            "title": "Blast Radius Analysis",
            "deployed_date": deployed_date,
            "batch": batch_info,
            "window_days": window_days,
            "z_threshold": z_threshold,
            "result_count": len(data),
        },
        "data": data,
    }


def get_deployment_diff(
    deployed_date: str,
    conn: duckdb.DuckDBPyConnection | None = None,
) -> dict:
    """
    Return all files changed across every commit in a deployment night,
    aggregated per file (summed lines, dominant change_type).
    """
    if conn is None:
        conn = get_ro_connection()

    # Per-commit breakdown (for the commit list)
    commits_cur = conn.execute("""
        SELECT
            c.commit_hash,
            c.author_name,
            c.committed_at,
            c.message,
            COUNT(cf.file_path)             AS files_changed,
            COALESCE(SUM(cf.lines_added),   0) AS lines_added,
            COALESCE(SUM(cf.lines_removed), 0) AS lines_removed
        FROM commits c
        LEFT JOIN commit_files cf ON cf.commit_hash = c.commit_hash
        WHERE CAST(c.deployed_at AS DATE) = CAST(? AS DATE)
        GROUP BY c.commit_hash, c.author_name, c.committed_at, c.message
        ORDER BY c.committed_at
    """, [deployed_date])
    commits_data = rows(commits_cur)

    # Aggregated per file across all commits that day
    files_cur = conn.execute("""
        SELECT
            cf.file_path,
            -- dominant change type: prefer A/D over M
            MAX(cf.change_type)             AS change_type,
            SUM(cf.lines_added)             AS lines_added,
            SUM(cf.lines_removed)           AS lines_removed,
            COUNT(DISTINCT cf.commit_hash)  AS touched_by_commits
        FROM commit_files cf
        JOIN commits c ON c.commit_hash = cf.commit_hash
        WHERE CAST(c.deployed_at AS DATE) = CAST(? AS DATE)
        GROUP BY cf.file_path
        ORDER BY SUM(cf.lines_added) + SUM(cf.lines_removed) DESC
    """, [deployed_date])
    files_data = rows(files_cur)

    return {
        "meta": {
            "deployed_date": deployed_date,
            "commit_count": len(commits_data),
            "file_count": len(files_data),
        },
        "commits": commits_data,
        "files": files_data,
    }


def get_hot_files(
    conn: duckdb.DuckDBPyConnection | None = None,
    window_days: int = 14,
    regression_pct: float = 10.0,
    top_n: int = 30,
) -> dict:
    """
    Files that most frequently appear in commits that caused latency regressions.

    A commit is flagged as a regression if, for at least one feature,
    the avg_ms in the post-deployment window exceeds the pre-deployment
    avg_ms by more than regression_pct%.
    """
    if conn is None:
        conn = get_ro_connection()

    cur = conn.execute("""
        WITH feature_impact AS (
            SELECT
                c.commit_hash,
                AVG(after_s.avg_ms) - AVG(before_s.avg_ms)                            AS delta_ms,
                (AVG(after_s.avg_ms) - AVG(before_s.avg_ms))
                    / NULLIF(AVG(before_s.avg_ms), 0) * 100                            AS delta_pct
            FROM commits c
            JOIN feature_daily_stats before_s
              ON before_s.stat_date >= CAST(c.deployed_at AS DATE) - INTERVAL (? || ' days')
             AND before_s.stat_date <  CAST(c.deployed_at AS DATE)
            JOIN feature_daily_stats after_s
              ON after_s.stat_date  >= CAST(c.deployed_at AS DATE)
             AND after_s.stat_date  <  CAST(c.deployed_at AS DATE) + INTERVAL (? || ' days')
             AND after_s.feature    =  before_s.feature
            WHERE c.deployed_at IS NOT NULL
            GROUP BY c.commit_hash, before_s.feature
            HAVING COUNT(before_s.stat_date) >= 3 AND COUNT(after_s.stat_date) >= 3
        ),
        regression_commits AS (
            SELECT DISTINCT commit_hash
            FROM feature_impact
            WHERE delta_pct > ?
        )
        SELECT
            cf.file_path,
            COUNT(DISTINCT cf.commit_hash)              AS regression_commits,
            SUM(cf.lines_added + cf.lines_removed)      AS total_churn_lines,
            SUM(cf.lines_added)                         AS total_lines_added,
            SUM(cf.lines_removed)                       AS total_lines_removed
        FROM commit_files cf
        JOIN regression_commits rc ON cf.commit_hash = rc.commit_hash
        GROUP BY cf.file_path
        ORDER BY regression_commits DESC, total_churn_lines DESC
        LIMIT ?
    """, [str(window_days), str(window_days), regression_pct, top_n])

    data = rows(cur)
    return {
        "meta": {
            "use_case": "UC-02",
            "title": "Hot Files",
            "window_days": window_days,
            "regression_pct_threshold": regression_pct,
        },
        "data": data,
    }


def get_deployment_summaries(
    from_date: str,
    to_date: str,
    conn: duckdb.DuckDBPyConnection | None = None,
    window_days: int = 14,
    z_threshold: float = 2.0,
) -> dict:
    """
    Lightweight blast-radius summary for every deployment night in a date range.
    Used to power the hover tooltip on the Feature Inspector chart.
    """
    if conn is None:
        conn = get_ro_connection()

    cur = conn.execute("""
        WITH dates AS (
            SELECT DISTINCT CAST(deployed_at AS DATE) AS deployed_date
            FROM commits
            WHERE CAST(deployed_at AS DATE) BETWEEN CAST(? AS DATE) AND CAST(? AS DATE)
        ),
        before_stats AS (
            SELECT
                d.deployed_date,
                fds.feature,
                AVG(fds.avg_ms)    AS before_avg,
                STDDEV(fds.avg_ms) AS before_std
            FROM dates d
            JOIN feature_daily_stats fds
              ON fds.stat_date >= d.deployed_date - INTERVAL (? || ' days')
             AND fds.stat_date <  d.deployed_date
            GROUP BY d.deployed_date, fds.feature
            HAVING COUNT(*) >= 3
        ),
        after_stats AS (
            SELECT
                d.deployed_date,
                fds.feature,
                AVG(fds.avg_ms) AS after_avg
            FROM dates d
            JOIN feature_daily_stats fds
              ON fds.stat_date >= d.deployed_date
             AND fds.stat_date <  d.deployed_date + INTERVAL (? || ' days')
            GROUP BY d.deployed_date, fds.feature
            HAVING COUNT(*) >= 3
        ),
        impact AS (
            SELECT
                b.deployed_date,
                b.feature,
                (a.after_avg - b.before_avg) / NULLIF(b.before_std, 0)        AS z_score,
                (a.after_avg - b.before_avg) / NULLIF(b.before_avg, 0) * 100  AS delta_pct
            FROM before_stats b
            JOIN after_stats a ON a.deployed_date = b.deployed_date AND a.feature = b.feature
        ),
        flagged AS (
            SELECT deployed_date, feature, delta_pct
            FROM impact
            WHERE ABS(z_score) >= ? AND delta_pct > 0
        ),
        worst_per_date AS (
            SELECT deployed_date, feature AS worst_feature, delta_pct AS worst_delta_pct
            FROM flagged
            QUALIFY ROW_NUMBER() OVER (PARTITION BY deployed_date ORDER BY delta_pct DESC) = 1
        ),
        summary AS (
            SELECT
                deployed_date,
                SUM(CASE WHEN ABS(z_score) >= ? THEN 1 ELSE 0 END) AS features_affected
            FROM impact
            GROUP BY deployed_date
        )
        SELECT
            s.deployed_date,
            s.features_affected,
            w.worst_feature,
            ROUND(w.worst_delta_pct, 1) AS worst_delta_pct
        FROM summary s
        LEFT JOIN worst_per_date w ON w.deployed_date = s.deployed_date
        ORDER BY s.deployed_date
    """, [from_date, to_date, str(window_days), str(window_days), z_threshold, z_threshold])

    return {"data": rows(cur)}


def get_author_impact(
    conn: duckdb.DuckDBPyConnection | None = None,
    window_days: int = 14,
    regression_pct: float = 10.0,
    top_n: int = 20,
) -> dict:
    """
    Per-author regression stats: how many of their commits caused latency regressions.
    """
    if conn is None:
        conn = get_ro_connection()

    cur = conn.execute("""
        WITH feature_impact AS (
            SELECT
                c.commit_hash,
                c.author_name,
                MAX(
                    (AVG(after_s.avg_ms) - AVG(before_s.avg_ms))
                    / NULLIF(AVG(before_s.avg_ms), 0) * 100
                ) OVER (PARTITION BY c.commit_hash) AS max_delta_pct
            FROM commits c
            JOIN feature_daily_stats before_s
              ON before_s.stat_date >= CAST(c.deployed_at AS DATE) - INTERVAL (? || ' days')
             AND before_s.stat_date <  CAST(c.deployed_at AS DATE)
            JOIN feature_daily_stats after_s
              ON after_s.stat_date  >= CAST(c.deployed_at AS DATE)
             AND after_s.stat_date  <  CAST(c.deployed_at AS DATE) + INTERVAL (? || ' days')
             AND after_s.feature    =  before_s.feature
            WHERE c.deployed_at IS NOT NULL
            GROUP BY c.commit_hash, c.author_name, before_s.feature
            HAVING COUNT(before_s.stat_date) >= 3 AND COUNT(after_s.stat_date) >= 3
        ),
        per_commit AS (
            SELECT DISTINCT commit_hash, author_name, max_delta_pct
            FROM feature_impact
        )
        SELECT
            author_name,
            COUNT(*)                                            AS total_commits,
            SUM(CASE WHEN max_delta_pct > ? THEN 1 ELSE 0 END) AS regression_commits,
            ROUND(
                SUM(CASE WHEN max_delta_pct > ? THEN 1 ELSE 0 END) * 100.0
                / NULLIF(COUNT(*), 0), 1
            )                                                   AS regression_rate_pct,
            ROUND(AVG(max_delta_pct), 2)                        AS avg_max_delta_pct
        FROM per_commit
        GROUP BY author_name
        HAVING total_commits >= 1
        ORDER BY regression_commits DESC, regression_rate_pct DESC
        LIMIT ?
    """, [str(window_days), str(window_days), regression_pct, regression_pct, top_n])

    data = rows(cur)
    return {
        "meta": {
            "use_case": "UC-02",
            "title": "Author Impact",
            "window_days": window_days,
            "regression_pct_threshold": regression_pct,
        },
        "data": data,
    }
