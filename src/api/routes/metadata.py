"""
src/api/routes/metadata.py

GET /api/metadata/features        — distinct features + types + event counts
GET /api/metadata/users           — distinct asmd_users
GET /api/metadata/workstations    — workstation list with office prefix
GET /api/metadata/offices         — distinct office prefixes
GET /api/metadata/date-range      — min/max mod_dt in audit_events
GET /api/metadata/commits         — recent commits for blast-radius selector
"""

from fastapi import APIRouter, Depends
import duckdb

from src.api.deps import get_db
from src.analytics._db import rows

router = APIRouter(tags=["metadata"])


@router.get("/features")
def list_features(conn: duckdb.DuckDBPyConnection = Depends(get_db)):
    cur = conn.execute("""
        SELECT
            feature_type,
            feature,
            SUM(event_count) AS event_count
        FROM feature_daily_stats
        GROUP BY feature_type, feature
        ORDER BY feature_type, feature
    """)
    return rows(cur)


@router.get("/users")
def list_users(conn: duckdb.DuckDBPyConnection = Depends(get_db)):
    cur = conn.execute("""
        SELECT DISTINCT asmd_user
        FROM audit_events
        ORDER BY asmd_user
    """)
    return [r["asmd_user"] for r in rows(cur)]


@router.get("/workstations")
def list_workstations(conn: duckdb.DuckDBPyConnection = Depends(get_db)):
    cur = conn.execute("""
        SELECT
            workstation,
            office_prefix,
            last_seen,
            total_events
        FROM workstation_profiles
        ORDER BY workstation
    """)
    return rows(cur)


@router.get("/offices")
def list_offices(conn: duckdb.DuckDBPyConnection = Depends(get_db)):
    cur = conn.execute("""
        SELECT DISTINCT SUBSTR(workstation, 1, 3) AS office_prefix
        FROM audit_events
        ORDER BY office_prefix
    """)
    return [r["office_prefix"] for r in rows(cur)]


@router.get("/date-range")
def date_range(conn: duckdb.DuckDBPyConnection = Depends(get_db)):
    cur = conn.execute("""
        SELECT
            MIN(mod_dt) AS min_date,
            MAX(mod_dt) AS max_date,
            COUNT(*)    AS total_events
        FROM audit_events
    """)
    return rows(cur)[0]


@router.get("/commits")
def list_commits(
    limit: int = 100,
    conn: duckdb.DuckDBPyConnection = Depends(get_db),
):
    cur = conn.execute("""
        SELECT
            commit_hash,
            repo_name,
            tag,
            author_name,
            committed_at,
            deployed_at,
            message
        FROM commits
        ORDER BY committed_at DESC
        LIMIT ?
    """, [limit])
    return rows(cur)
