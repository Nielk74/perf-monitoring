"""
src/api/routes/ingestion.py

GET  /api/ingestion/status          — row counts + last import times
POST /api/ingestion/run/oracle      — trigger Oracle/SQLite import
POST /api/ingestion/run/git         — trigger Git import
POST /api/ingestion/run/materialize — rebuild derived tables
POST /api/ingestion/deployment      — register a deployment event
"""

import threading
from datetime import datetime, timezone
from typing import Any

from fastapi import APIRouter, BackgroundTasks, Body, HTTPException
from pydantic import BaseModel

from src.analytics._db import rows
from src.api.deps import get_db
from src.config import settings

router = APIRouter(tags=["ingestion"])

# In-memory job status tracker
_jobs: dict[str, dict[str, Any]] = {
    "oracle":      {"status": "idle", "last_run": None, "last_result": None},
    "git":         {"status": "idle", "last_run": None, "last_result": None},
    "materialize": {"status": "idle", "last_run": None, "last_result": None},
}
_jobs_lock = threading.Lock()


def _set_job(name: str, **kwargs) -> None:
    with _jobs_lock:
        _jobs[name].update(kwargs)


# ---------------------------------------------------------------------------
# Status
# ---------------------------------------------------------------------------

@router.get("/status")
def ingestion_status():
    conn = get_db()
    ae = conn.execute("""
        SELECT COUNT(*) AS row_count, MAX(mod_dt) AS max_mod_dt
        FROM audit_events
    """).fetchone()
    cm = conn.execute("""
        SELECT COUNT(*) AS row_count, MAX(committed_at) AS last_committed_at
        FROM commits
    """).fetchone()
    mat = conn.execute("""
        SELECT MAX(stat_date) AS last_materialized
        FROM feature_daily_stats
    """).fetchone()

    with _jobs_lock:
        jobs_snapshot = {k: dict(v) for k, v in _jobs.items()}

    return {
        "audit_events": {
            "row_count":  ae[0],
            "max_mod_dt": str(ae[1]) if ae[1] else None,
        },
        "commits": {
            "row_count":        cm[0],
            "last_committed_at": str(cm[1]) if cm[1] else None,
        },
        "derived_tables": {
            "last_materialized": str(mat[0]) if mat[0] else None,
        },
        "jobs": jobs_snapshot,
    }


# ---------------------------------------------------------------------------
# Oracle / SQLite import
# ---------------------------------------------------------------------------

def _run_oracle_import() -> None:
    _set_job("oracle", status="running", last_run=datetime.now(timezone.utc).isoformat())
    try:
        from src.ingestion.oracle_importer import SqliteSource, run_import
        conn = get_db()
        source = SqliteSource(settings.oracle.mock_db)
        result = run_import(source, conn, days_limit=None, dry_run=False)
        _set_job("oracle", status="idle", last_result=result)
    except Exception as exc:
        _set_job("oracle", status="error", last_result={"error": str(exc)})


@router.post("/run/oracle")
def trigger_oracle(background_tasks: BackgroundTasks):
    with _jobs_lock:
        if _jobs["oracle"]["status"] == "running":
            raise HTTPException(409, detail="Oracle import already running")
    background_tasks.add_task(_run_oracle_import)
    return {"message": "Oracle import started"}


# ---------------------------------------------------------------------------
# Git import
# ---------------------------------------------------------------------------

def _run_git_import() -> None:
    _set_job("git", status="running", last_run=datetime.now(timezone.utc).isoformat())
    try:
        from src.ingestion.git_importer import import_repo, load_repos
        repos = load_repos("config/settings.yaml", repo_filter=None)
        conn = get_db()
        results = []
        for repo_cfg in repos:
            r = import_repo(repo_cfg, conn, max_commits=None, dry_run=False)
            results.append(r)
        _set_job("git", status="idle", last_result=results)
    except Exception as exc:
        _set_job("git", status="error", last_result={"error": str(exc)})


@router.post("/run/git")
def trigger_git(background_tasks: BackgroundTasks):
    with _jobs_lock:
        if _jobs["git"]["status"] == "running":
            raise HTTPException(409, detail="Git import already running")
    background_tasks.add_task(_run_git_import)
    return {"message": "Git import started"}


# ---------------------------------------------------------------------------
# Materialize
# ---------------------------------------------------------------------------

def _run_materialize() -> None:
    _set_job("materialize", status="running", last_run=datetime.now(timezone.utc).isoformat())
    try:
        from src.analytics.materializer import materialize_all
        conn = get_db()
        materialize_all(conn)
        _set_job("materialize", status="idle", last_result={"ok": True})
    except Exception as exc:
        _set_job("materialize", status="error", last_result={"error": str(exc)})


@router.post("/run/materialize")
def trigger_materialize(background_tasks: BackgroundTasks):
    with _jobs_lock:
        if _jobs["materialize"]["status"] == "running":
            raise HTTPException(409, detail="Materialize already running")
    background_tasks.add_task(_run_materialize)
    return {"message": "Materialize started"}


# ---------------------------------------------------------------------------
# Deployment registration
# ---------------------------------------------------------------------------

class DeploymentIn(BaseModel):
    commit_hash: str
    deployed_at: str   # ISO datetime string


@router.post("/deployment")
def register_deployment(body: DeploymentIn = Body(...)):
    conn = get_db()
    updated = conn.execute("""
        UPDATE commits
        SET deployed_at = ?
        WHERE commit_hash = ? OR commit_hash LIKE ?
        RETURNING commit_hash, tag, deployed_at
    """, [body.deployed_at, body.commit_hash, body.commit_hash + "%"]).fetchall()

    if not updated:
        raise HTTPException(404, detail=f"No commit found matching hash '{body.commit_hash}'")

    return {
        "updated": len(updated),
        "commits": [{"commit_hash": r[0], "tag": r[1], "deployed_at": str(r[2])} for r in updated],
    }
