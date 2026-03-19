"""
src/analytics/materializer.py

Rebuilds all derived tables from raw audit_events data.
Run nightly after ingestion, or manually after a large import.

Usage:
    python -m src.analytics.materializer
"""

import sys
import time

import duckdb

from src.db.connection import get_connection


def materialize_all(conn: duckdb.DuckDBPyConnection) -> None:
    t0 = time.monotonic()
    print("Materializing derived tables...")

    materialize_feature_daily_stats(conn)
    materialize_feature_baselines(conn)
    materialize_user_sessions(conn)
    materialize_feature_sequences(conn)
    materialize_workstation_profiles(conn)

    print(f"Done in {time.monotonic() - t0:.1f}s")


def materialize_feature_daily_stats(conn: duckdb.DuckDBPyConnection) -> None:
    t0 = time.monotonic()
    conn.execute("DELETE FROM feature_daily_stats")
    conn.execute("""
        INSERT INTO feature_daily_stats
        SELECT
            CAST(mod_dt AS DATE)                                                                            AS stat_date,
            feature_type,
            feature,
            COUNT(*)                                                                                        AS event_count,
            COUNT(DISTINCT asmd_user)                                                                       AS distinct_users,
            COUNT(DISTINCT workstation)                                                                     AS distinct_workstations,
            PERCENTILE_CONT(0.50) WITHIN GROUP (ORDER BY duration_ms) FILTER (WHERE duration_ms IS NOT NULL) AS p50_ms,
            PERCENTILE_CONT(0.95) WITHIN GROUP (ORDER BY duration_ms) FILTER (WHERE duration_ms IS NOT NULL) AS p95_ms,
            PERCENTILE_CONT(0.99) WITHIN GROUP (ORDER BY duration_ms) FILTER (WHERE duration_ms IS NOT NULL) AS p99_ms,
            AVG(duration_ms) FILTER (WHERE duration_ms IS NOT NULL)                                         AS avg_ms,
            SUM(duration_ms)                                                                                AS total_wait_ms
        FROM audit_events
        GROUP BY CAST(mod_dt AS DATE), feature_type, feature
    """)
    n = conn.execute("SELECT COUNT(*) FROM feature_daily_stats").fetchone()[0]
    print(f"  feature_daily_stats: {n:,} rows ({time.monotonic()-t0:.1f}s)")


def materialize_feature_baselines(conn: duckdb.DuckDBPyConnection) -> None:
    """Rolling 8-week baseline per feature: mean, std, and weekly regression slope."""
    t0 = time.monotonic()
    conn.execute("DELETE FROM feature_baselines")
    conn.execute("""
        INSERT INTO feature_baselines
        SELECT
            feature,
            feature_type,
            CURRENT_DATE                                                                AS baseline_date,
            8                                                                           AS window_weeks,
            AVG(event_count)                                                            AS mean_daily_count,
            STDDEV(event_count)                                                         AS std_daily_count,
            AVG(p95_ms)                                                                 AS mean_p95_ms,
            STDDEV(p95_ms)                                                              AS std_p95_ms,
            AVG(avg_ms)                                                                 AS mean_avg_ms,
            STDDEV(avg_ms)                                                              AS std_avg_ms,
            -- slope in ms per week (X = week number since epoch)
            REGR_SLOPE(avg_ms, epoch(CAST(stat_date AS TIMESTAMP)) / 604800.0)         AS weekly_slope_ms,
            REGR_SLOPE(avg_ms, epoch(CAST(stat_date AS TIMESTAMP)) / 604800.0)
                / NULLIF(AVG(avg_ms), 0) * 100                                         AS weekly_slope_pct
        FROM feature_daily_stats
        WHERE stat_date >= CURRENT_DATE - INTERVAL '8 weeks'
          AND avg_ms IS NOT NULL
          AND event_count >= 5
        GROUP BY feature, feature_type
        HAVING COUNT(*) >= 7
    """)
    n = conn.execute("SELECT COUNT(*) FROM feature_baselines").fetchone()[0]
    print(f"  feature_baselines:   {n:,} rows ({time.monotonic()-t0:.1f}s)")


def materialize_user_sessions(conn: duckdb.DuckDBPyConnection) -> None:
    """Reconstruct sessions: new session = >30 min gap within user+workstation."""
    t0 = time.monotonic()
    conn.execute("DELETE FROM user_sessions")
    conn.execute("""
        INSERT INTO user_sessions
        WITH lagged AS (
            SELECT
                asmd_user,
                supp_user,
                workstation,
                mod_dt,
                LAG(mod_dt) OVER (PARTITION BY asmd_user, workstation ORDER BY mod_dt) AS prev_dt
            FROM audit_events
        ),
        with_session_num AS (
            SELECT
                asmd_user,
                supp_user,
                workstation,
                mod_dt,
                SUM(
                    CASE WHEN prev_dt IS NULL
                              OR epoch(mod_dt) - epoch(prev_dt) > 1800
                         THEN 1 ELSE 0
                    END
                ) OVER (PARTITION BY asmd_user, workstation ORDER BY mod_dt) AS session_num
            FROM lagged
        )
        SELECT
            ROW_NUMBER() OVER (ORDER BY MIN(mod_dt))            AS session_id,
            asmd_user,
            MIN(supp_user)                                      AS supp_user,
            workstation,
            MIN(mod_dt)                                         AS session_start,
            MAX(mod_dt)                                         AS session_end,
            COUNT(*)                                            AS event_count,
            BOOL_OR(supp_user != asmd_user)                    AS is_impersonated,
            (epoch(MAX(mod_dt)) - epoch(MIN(mod_dt))) / 60.0   AS duration_minutes
        FROM with_session_num
        GROUP BY asmd_user, workstation, session_num
    """)
    n = conn.execute("SELECT COUNT(*) FROM user_sessions").fetchone()[0]
    print(f"  user_sessions:       {n:,} rows ({time.monotonic()-t0:.1f}s)")


def materialize_feature_sequences(conn: duckdb.DuckDBPyConnection) -> None:
    """Consecutive feature transitions within sessions (30-min gap = session boundary)."""
    t0 = time.monotonic()
    conn.execute("DELETE FROM feature_sequences")
    conn.execute("""
        INSERT INTO feature_sequences
        WITH ordered_events AS (
            SELECT
                asmd_user,
                workstation,
                mod_dt,
                feature,
                feature_type,
                LAG(feature)      OVER (PARTITION BY asmd_user, workstation ORDER BY mod_dt) AS prev_feature,
                LAG(feature_type) OVER (PARTITION BY asmd_user, workstation ORDER BY mod_dt) AS prev_feature_type,
                LAG(mod_dt)       OVER (PARTITION BY asmd_user, workstation ORDER BY mod_dt) AS prev_dt
            FROM audit_events
            WHERE supp_user = asmd_user  -- exclude support sessions from workflow patterns
        ),
        transitions AS (
            SELECT
                prev_feature                                AS from_feature,
                feature                                     AS to_feature,
                prev_feature_type                           AS from_feature_type,
                feature_type                                AS to_feature_type,
                epoch(mod_dt) - epoch(prev_dt)              AS gap_seconds
            FROM ordered_events
            WHERE prev_feature IS NOT NULL
              AND epoch(mod_dt) - epoch(prev_dt) < 1800   -- within same session
              AND prev_feature != feature                  -- no self-transitions
        )
        SELECT
            from_feature,
            to_feature,
            from_feature_type,
            to_feature_type,
            COUNT(*)            AS transition_count,
            AVG(gap_seconds)    AS avg_gap_seconds
        FROM transitions
        GROUP BY from_feature, to_feature, from_feature_type, to_feature_type
    """)
    n = conn.execute("SELECT COUNT(*) FROM feature_sequences").fetchone()[0]
    print(f"  feature_sequences:   {n:,} rows ({time.monotonic()-t0:.1f}s)")


def materialize_workstation_profiles(conn: duckdb.DuckDBPyConnection) -> None:
    t0 = time.monotonic()
    conn.execute("DELETE FROM workstation_profiles")
    conn.execute("""
        INSERT INTO workstation_profiles
        SELECT
            workstation,
            SUBSTR(workstation, 1, 3)                   AS office_prefix,
            MAX(CAST(mod_dt AS DATE))                   AS last_seen,
            COUNT(*)                                    AS total_events,
            AVG(duration_ms) FILTER (WHERE feature = 'Star Startup' AND duration_ms IS NOT NULL)
                                                        AS avg_startup_ms,
            PERCENTILE_CONT(0.95) WITHIN GROUP (ORDER BY duration_ms)
                FILTER (WHERE feature = 'Star Startup' AND duration_ms IS NOT NULL)
                                                        AS p95_startup_ms
        FROM audit_events
        GROUP BY workstation
    """)
    n = conn.execute("SELECT COUNT(*) FROM workstation_profiles").fetchone()[0]
    print(f"  workstation_profiles:{n:,} rows ({time.monotonic()-t0:.1f}s)")


if __name__ == "__main__":
    conn = get_connection()
    materialize_all(conn)
    conn.close()
    sys.exit(0)
