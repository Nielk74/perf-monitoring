"""
src/analytics/impersonation_audit.py — UC-04 Impersonation Audit

Surfaces all support sessions (SUPP_USER != ASMD_USER), ranked by recency
and duration. Also provides per-support-user and per-target-user summaries.
"""

from __future__ import annotations

import duckdb

from ._db import get_ro_connection, rows


def get_support_sessions(
    conn: duckdb.DuckDBPyConnection | None = None,
    days: int = 90,
    supp_user: str | None = None,
    asmd_user: str | None = None,
    limit: int = 200,
) -> dict:
    """
    List recent impersonation sessions.

    Parameters
    ----------
    days : int
        Rolling window in days.
    supp_user : str or None
        Filter to a specific support agent.
    asmd_user : str or None
        Filter to a specific impersonated user.
    limit : int
        Max rows returned.
    """
    if conn is None:
        conn = get_ro_connection()

    conditions = [
        "is_impersonated = TRUE",
        f"session_start >= CURRENT_TIMESTAMP - INTERVAL '{days} days'",
    ]
    params: list = []
    if supp_user:
        conditions.append("supp_user = ?")
        params.append(supp_user)
    if asmd_user:
        conditions.append("asmd_user = ?")
        params.append(asmd_user)
    params.append(limit)

    where = " AND ".join(conditions)

    cur = conn.execute(f"""
        SELECT
            session_id,
            asmd_user,
            supp_user,
            workstation,
            session_start,
            session_end,
            event_count,
            duration_minutes
        FROM user_sessions
        WHERE {where}
        ORDER BY session_start DESC
        LIMIT ?
    """, params)

    data = rows(cur)

    return {
        "meta": {
            "use_case": "UC-04",
            "title": "Impersonation Sessions",
            "days": days,
            "supp_user_filter": supp_user,
            "asmd_user_filter": asmd_user,
            "result_count": len(data),
        },
        "data": data,
    }


def get_support_agent_summary(
    conn: duckdb.DuckDBPyConnection | None = None,
    days: int = 90,
) -> dict:
    """Per-support-agent: session count, distinct users helped, total duration."""
    if conn is None:
        conn = get_ro_connection()

    cur = conn.execute("""
        SELECT
            supp_user,
            COUNT(*)                        AS session_count,
            COUNT(DISTINCT asmd_user)       AS distinct_users_helped,
            SUM(duration_minutes)           AS total_minutes,
            AVG(duration_minutes)           AS avg_session_minutes,
            MAX(session_start)              AS last_session
        FROM user_sessions
        WHERE is_impersonated = TRUE
          AND session_start >= CURRENT_TIMESTAMP - INTERVAL (? || ' days')
        GROUP BY supp_user
        ORDER BY session_count DESC
    """, [str(days)])

    return {
        "meta": {
            "use_case": "UC-04",
            "title": "Support Agent Summary",
            "days": days,
        },
        "data": rows(cur),
    }


def get_impersonation_timeline(
    conn: duckdb.DuckDBPyConnection | None = None,
    days: int = 90,
) -> dict:
    """Daily count of impersonation sessions and distinct users affected."""
    if conn is None:
        conn = get_ro_connection()

    cur = conn.execute("""
        SELECT
            CAST(session_start AS DATE)         AS day,
            COUNT(*)                            AS session_count,
            COUNT(DISTINCT asmd_user)           AS distinct_users,
            COUNT(DISTINCT supp_user)           AS distinct_agents,
            SUM(duration_minutes)               AS total_minutes
        FROM user_sessions
        WHERE is_impersonated = TRUE
          AND session_start >= CURRENT_TIMESTAMP - INTERVAL (? || ' days')
        GROUP BY CAST(session_start AS DATE)
        ORDER BY day
    """, [str(days)])

    return {
        "meta": {
            "use_case": "UC-04",
            "title": "Impersonation Timeline",
            "days": days,
        },
        "data": rows(cur),
    }


def get_features_accessed_during_support(
    conn: duckdb.DuckDBPyConnection | None = None,
    days: int = 90,
    top_n: int = 30,
) -> dict:
    """Which features are most frequently accessed during impersonation sessions."""
    if conn is None:
        conn = get_ro_connection()

    cur = conn.execute("""
        SELECT
            feature,
            feature_type,
            COUNT(*)                    AS event_count,
            COUNT(DISTINCT asmd_user)   AS distinct_users,
            AVG(duration_ms) FILTER (WHERE duration_ms IS NOT NULL) AS avg_ms
        FROM audit_events
        WHERE supp_user != asmd_user
          AND mod_dt >= CURRENT_TIMESTAMP - INTERVAL (? || ' days')
        GROUP BY feature, feature_type
        ORDER BY event_count DESC
        LIMIT ?
    """, [str(days), top_n])

    return {
        "meta": {
            "use_case": "UC-04",
            "title": "Features Accessed During Support",
            "days": days,
            "top_n": top_n,
        },
        "data": rows(cur),
    }
