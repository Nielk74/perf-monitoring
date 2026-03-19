"""
src/analytics/adoption_velocity.py — UC-08 Adoption Velocity

Tracks how quickly new features spread across the user base after release.
Uses a weekly rolling distinct_users count to model S-curve adoption.
"""

from __future__ import annotations

import duckdb

from ._db import get_ro_connection, rows


def get_adoption_curve(
    feature: str,
    conn: duckdb.DuckDBPyConnection | None = None,
    days: int = 180,
) -> dict:
    """
    Weekly distinct-user count for a feature since its first appearance.

    Parameters
    ----------
    feature : str
        Feature name to track.
    days : int
        Max window to look back.

    Returns
    -------
    dict with meta + data (week, cumulative_users, new_users, event_count)
    """
    if conn is None:
        conn = get_ro_connection()

    cur = conn.execute("""
        WITH weekly AS (
            SELECT
                DATE_TRUNC('week', stat_date)   AS week_start,
                SUM(event_count)                AS event_count,
                -- distinct_users is daily; we take max as proxy for weekly reach
                MAX(distinct_users)             AS weekly_users
            FROM feature_daily_stats
            WHERE feature = ?
              AND stat_date >= CURRENT_DATE - INTERVAL (? || ' days')
            GROUP BY DATE_TRUNC('week', stat_date)
        )
        SELECT
            week_start,
            event_count,
            weekly_users,
            SUM(weekly_users) OVER (ORDER BY week_start) AS cumulative_user_weeks
        FROM weekly
        ORDER BY week_start
    """, [feature, str(days)])

    data = rows(cur)

    return {
        "meta": {
            "use_case": "UC-08",
            "title": "Adoption Velocity",
            "feature": feature,
            "days": days,
        },
        "data": data,
    }


def get_new_features(
    conn: duckdb.DuckDBPyConnection | None = None,
    introduced_within_days: int = 90,
    min_events: int = 5,
) -> dict:
    """
    Features first seen within the last N days — candidate new features to track.
    """
    if conn is None:
        conn = get_ro_connection()

    cur = conn.execute("""
        SELECT
            feature,
            feature_type,
            MIN(stat_date)          AS first_seen,
            MAX(stat_date)          AS last_seen,
            SUM(event_count)        AS total_events,
            MAX(distinct_users)     AS peak_daily_users,
            CURRENT_DATE - MIN(stat_date) AS age_days
        FROM feature_daily_stats
        GROUP BY feature, feature_type
        HAVING MIN(stat_date) >= CURRENT_DATE - INTERVAL (? || ' days')
           AND SUM(event_count) >= ?
        ORDER BY first_seen DESC
    """, [str(introduced_within_days), min_events])

    return {
        "meta": {
            "use_case": "UC-08",
            "title": "New Features",
            "introduced_within_days": introduced_within_days,
            "min_events": min_events,
        },
        "data": rows(cur),
    }


def get_feature_type_adoption(
    conn: duckdb.DuckDBPyConnection | None = None,
    days: int = 90,
) -> dict:
    """
    Weekly active users per feature_type — shows which modules are growing vs shrinking.
    """
    if conn is None:
        conn = get_ro_connection()

    cur = conn.execute("""
        SELECT
            DATE_TRUNC('week', stat_date)   AS week_start,
            feature_type,
            SUM(event_count)                AS event_count,
            MAX(distinct_users)             AS weekly_users
        FROM feature_daily_stats
        WHERE stat_date >= CURRENT_DATE - INTERVAL (? || ' days')
        GROUP BY DATE_TRUNC('week', stat_date), feature_type
        ORDER BY week_start, feature_type
    """, [str(days)])

    return {
        "meta": {
            "use_case": "UC-08",
            "title": "Feature Type Adoption",
            "days": days,
        },
        "data": rows(cur),
    }
