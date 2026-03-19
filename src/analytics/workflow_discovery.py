"""
src/analytics/workflow_discovery.py — UC-07 Workflow Discovery

Analyses feature transition sequences within sessions to surface the most
common workflows and unexpected navigation patterns.
Uses the pre-computed feature_sequences table.
"""

from __future__ import annotations

import duckdb

from ._db import get_ro_connection, rows


def get_top_transitions(
    conn: duckdb.DuckDBPyConnection | None = None,
    from_feature: str | None = None,
    to_feature: str | None = None,
    feature_type: str | None = None,
    top_n: int = 50,
    min_count: int = 5,
) -> dict:
    """
    Most frequent feature→feature transitions (session-aware, support excluded).

    Parameters
    ----------
    from_feature : str or None
        Filter to transitions starting from this feature.
    to_feature : str or None
        Filter to transitions ending at this feature.
    feature_type : str or None
        Restrict both from/to to this feature_type.
    top_n : int
        Max rows returned.
    min_count : int
        Minimum transition count to include.
    """
    if conn is None:
        conn = get_ro_connection()

    conditions = [f"transition_count >= {min_count}"]
    params: list = []

    if from_feature:
        conditions.append("from_feature = ?")
        params.append(from_feature)
    if to_feature:
        conditions.append("to_feature = ?")
        params.append(to_feature)
    if feature_type:
        conditions.append("from_feature_type = ? AND to_feature_type = ?")
        params.extend([feature_type, feature_type])
    params.append(top_n)

    where = " AND ".join(conditions)

    cur = conn.execute(f"""
        SELECT
            from_feature,
            to_feature,
            from_feature_type,
            to_feature_type,
            transition_count,
            avg_gap_seconds,
            avg_gap_seconds / 60.0 AS avg_gap_minutes
        FROM feature_sequences
        WHERE {where}
        ORDER BY transition_count DESC
        LIMIT ?
    """, params)

    data = rows(cur)

    return {
        "meta": {
            "use_case": "UC-07",
            "title": "Feature Transition Heatmap",
            "from_feature": from_feature,
            "to_feature": to_feature,
            "feature_type": feature_type,
            "min_count": min_count,
            "top_n": top_n,
            "result_count": len(data),
        },
        "data": data,
    }


def get_workflow_graph(
    conn: duckdb.DuckDBPyConnection | None = None,
    min_count: int = 10,
    max_edges: int = 100,
) -> dict:
    """
    Returns nodes + edges suitable for a force-directed graph visualisation.
    Nodes = features, edges = transitions weighted by count.
    """
    if conn is None:
        conn = get_ro_connection()

    edge_cur = conn.execute("""
        SELECT
            from_feature,
            to_feature,
            from_feature_type,
            to_feature_type,
            transition_count,
            avg_gap_seconds
        FROM feature_sequences
        WHERE transition_count >= ?
        ORDER BY transition_count DESC
        LIMIT ?
    """, [min_count, max_edges])

    edges = rows(edge_cur)

    # Derive unique nodes from edges
    node_set: dict[str, str] = {}
    for e in edges:
        node_set[e["from_feature"]] = e["from_feature_type"]
        node_set[e["to_feature"]]   = e["to_feature_type"]

    nodes = [
        {"feature": f, "feature_type": t}
        for f, t in sorted(node_set.items())
    ]

    return {
        "meta": {
            "use_case": "UC-07",
            "title": "Workflow Graph",
            "min_count": min_count,
            "max_edges": max_edges,
            "node_count": len(nodes),
            "edge_count": len(edges),
        },
        "nodes": nodes,
        "edges": edges,
    }


def get_session_summary(
    conn: duckdb.DuckDBPyConnection | None = None,
    days: int = 30,
) -> dict:
    """
    Aggregate session statistics: distribution of session lengths and event counts.
    """
    if conn is None:
        conn = get_ro_connection()

    cur = conn.execute("""
        SELECT
            COUNT(*)                            AS total_sessions,
            AVG(duration_minutes)               AS avg_duration_minutes,
            PERCENTILE_CONT(0.50) WITHIN GROUP (ORDER BY duration_minutes) AS p50_minutes,
            PERCENTILE_CONT(0.95) WITHIN GROUP (ORDER BY duration_minutes) AS p95_minutes,
            AVG(event_count)                    AS avg_events_per_session,
            PERCENTILE_CONT(0.95) WITHIN GROUP (ORDER BY event_count)      AS p95_events,
            COUNT(DISTINCT asmd_user)           AS distinct_users,
            SUM(CASE WHEN is_impersonated THEN 1 ELSE 0 END) AS impersonated_sessions
        FROM user_sessions
        WHERE session_start >= CURRENT_TIMESTAMP - INTERVAL (? || ' days')
    """, [str(days)])

    return {
        "meta": {"use_case": "UC-07", "title": "Session Summary", "days": days},
        "data": rows(cur),
    }
