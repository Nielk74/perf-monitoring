"""
src/ingestion/oracle_importer.py

Imports rows from STAR_ACTION_AUDIT into the local DuckDB audit_events table.

In production, the source is Oracle (via python-oracledb thin mode).
In development, the source is the SQLite mock produced by mock_oracle.py.

Usage:
    python -m src.ingestion.oracle_importer                  # incremental from high-water mark
    python -m src.ingestion.oracle_importer --days 30        # force reload last N days
    python -m src.ingestion.oracle_importer --dry-run        # count rows, no write
    python -m src.ingestion.oracle_importer --mock           # force SQLite source
"""

import argparse
import hashlib
import sqlite3
import sys
import time
from datetime import datetime, timedelta, timezone
from typing import Iterator

import duckdb

from src.db.connection import get_connection

BATCH_SIZE = 100_000
ORACLE_ARRAY_SIZE = 50_000   # rows fetched per network round trip
DEFAULT_MOCK_PATH = "data/mock_oracle.db"


# ---------------------------------------------------------------------------
# Row key (deduplication)
# ---------------------------------------------------------------------------

def row_key(supp_user, asmd_user, workstation, mod_dt, feature, duration_ms) -> int:
    sig = f"{supp_user}|{asmd_user}|{workstation}|{mod_dt}|{feature}|{duration_ms}"
    return int(hashlib.md5(sig.encode()).hexdigest(), 16) % (2**63)


# ---------------------------------------------------------------------------
# Source abstraction
# ---------------------------------------------------------------------------

class SqliteSource:
    """Reads from the SQLite mock database."""

    def __init__(self, db_path: str):
        self.db_path = db_path

    def fetch(self, high_water_mark: datetime, days_limit: int | None) -> Iterator[dict]:
        conn = sqlite3.connect(self.db_path)
        conn.row_factory = sqlite3.Row

        cutoff = _cutoff_dt(days_limit)
        effective_hwm = max(high_water_mark, cutoff) if cutoff else high_water_mark

        rows = conn.execute("""
            SELECT SUPP_USER, ASMD_USER, WORKSTATION, MOD_DT,
                   FEATURE_TYPE, FEATURE, DETAIL, DURATION_MS
            FROM STAR_ACTION_AUDIT
            WHERE MOD_DT > ?
            ORDER BY MOD_DT ASC
        """, (effective_hwm.isoformat(),))

        for row in rows:
            yield dict(row)

        conn.close()

    def count(self, high_water_mark: datetime, days_limit: int | None) -> int:
        conn = sqlite3.connect(self.db_path)
        cutoff = _cutoff_dt(days_limit)
        effective_hwm = max(high_water_mark, cutoff) if cutoff else high_water_mark
        n = conn.execute(
            "SELECT COUNT(*) FROM STAR_ACTION_AUDIT WHERE MOD_DT > ?",
            (effective_hwm.isoformat(),)
        ).fetchone()[0]
        conn.close()
        return n


class OracleSource:
    """Reads from the real Oracle STAR.STAR_ACTION_AUDIT (production)."""

    def __init__(self, user: str, password: str, dsn: str):
        try:
            import oracledb
        except ImportError:
            raise RuntimeError("python-oracledb is not installed. Run: pip install python-oracledb")
        self._oracledb = oracledb
        self.user = user
        self.password = password
        self.dsn = dsn

    def _connect(self):
        return self._oracledb.connect(user=self.user, password=self.password, dsn=self.dsn)

    def fetch(self, high_water_mark: datetime, days_limit: int | None) -> Iterator[dict]:
        conn = self._connect()
        cursor = conn.cursor()
        cursor.arraysize = ORACLE_ARRAY_SIZE
        cutoff_expr = f"SYSTIMESTAMP - {days_limit or 365}"
        cursor.execute(f"""
            SELECT SUPP_USER, ASMD_USER, WORKSTATION,
                   CAST(MOD_DT AS TIMESTAMP) AS MOD_DT,
                   FEATURE_TYPE, FEATURE, DETAIL, DURATION_MS
            FROM STAR.STAR_ACTION_AUDIT
            WHERE MOD_DT > :hwm
              AND MOD_DT > {cutoff_expr}
        """, hwm=high_water_mark)

        cols = [c[0].lower() for c in cursor.description]
        while True:
            chunk = cursor.fetchmany()
            if not chunk:
                break
            for row in chunk:
                yield dict(zip(cols, row))

        cursor.close()
        conn.close()

    def count(self, high_water_mark: datetime, days_limit: int | None) -> int:
        conn = self._connect()
        cursor = conn.cursor()
        cutoff_expr = f"SYSTIMESTAMP - {days_limit or 365}"
        cursor.execute(f"""
            SELECT COUNT(*) FROM STAR.STAR_ACTION_AUDIT
            WHERE MOD_DT > :hwm AND MOD_DT > {cutoff_expr}
        """, hwm=high_water_mark)
        n = cursor.fetchone()[0]
        cursor.close()
        conn.close()
        return n


# ---------------------------------------------------------------------------
# High-water mark
# ---------------------------------------------------------------------------

def _get_high_water_mark(duckdb_conn: duckdb.DuckDBPyConnection) -> datetime:
    """Return MAX(mod_dt) from audit_events, or 12 months ago if table is empty."""
    row = duckdb_conn.execute("SELECT MAX(mod_dt) FROM audit_events").fetchone()
    if row and row[0]:
        hwm = row[0]
        if hwm.tzinfo is None:
            hwm = hwm.replace(tzinfo=timezone.utc)
        return hwm
    return datetime.now(timezone.utc) - timedelta(days=365)


def _cutoff_dt(days_limit: int | None) -> datetime | None:
    if days_limit is None:
        return None
    return datetime.now(timezone.utc) - timedelta(days=days_limit)


# ---------------------------------------------------------------------------
# Batch insert
# ---------------------------------------------------------------------------

def _insert_batch(duckdb_conn: duckdb.DuckDBPyConnection, rows: list[dict]) -> int:
    """Insert a batch of rows. Returns number of rows inserted (skipping duplicates)."""
    params = []
    for r in rows:
        mod_dt = r["MOD_DT"] if "MOD_DT" in r else r["mod_dt"]
        supp   = r.get("SUPP_USER") or r.get("supp_user")
        asmd   = r.get("ASMD_USER") or r.get("asmd_user")
        ws     = r.get("WORKSTATION") or r.get("workstation")
        ftype  = r.get("FEATURE_TYPE") or r.get("feature_type")
        feat   = r.get("FEATURE") or r.get("feature")
        detail = r.get("DETAIL") or r.get("detail")
        dur    = r.get("DURATION_MS") or r.get("duration_ms")

        rid = row_key(supp, asmd, ws, mod_dt, feat, dur)
        params.append((rid, supp, asmd, ws, mod_dt, ftype, feat, detail, dur))

    duckdb_conn.executemany("""
        INSERT OR IGNORE INTO audit_events
            (id, supp_user, asmd_user, workstation, mod_dt,
             feature_type, feature, detail, duration_ms)
        VALUES (?, ?, ?, ?, ?, ?, ?, ?, ?)
    """, params)

    return len(params)


# ---------------------------------------------------------------------------
# Main import logic
# ---------------------------------------------------------------------------

def run_import(
    source,
    duckdb_conn: duckdb.DuckDBPyConnection,
    days_limit: int | None = None,
    dry_run: bool = False,
) -> dict:
    t0 = time.monotonic()
    hwm = _get_high_water_mark(duckdb_conn)
    print(f"High-water mark: {hwm.isoformat()}")

    if dry_run:
        n = source.count(hwm, days_limit)
        print(f"Dry run: {n:,} rows would be imported.")
        return {"rows": n, "elapsed_s": 0, "dry_run": True}

    batch: list[dict] = []
    total = 0

    for row in source.fetch(hwm, days_limit):
        batch.append(row)
        if len(batch) >= BATCH_SIZE:
            _insert_batch(duckdb_conn, batch)
            total += len(batch)
            batch = []
            print(f"  ... {total:,} rows imported", end="\r")

    if batch:
        _insert_batch(duckdb_conn, batch)
        total += len(batch)

    elapsed = time.monotonic() - t0
    print(f"Imported {total:,} rows in {elapsed:.1f}s")
    return {"rows": total, "elapsed_s": elapsed, "dry_run": False}


# ---------------------------------------------------------------------------
# Entry point
# ---------------------------------------------------------------------------

if __name__ == "__main__":
    parser = argparse.ArgumentParser(description="Import STAR_ACTION_AUDIT into DuckDB")
    parser.add_argument("--days",    type=int,  default=None, help="Reload last N days (overrides high-water mark)")
    parser.add_argument("--dry-run", action="store_true",     help="Count rows only, no write")
    parser.add_argument("--mock",    action="store_true",     help="Force SQLite source (default when Oracle not configured)")
    parser.add_argument("--mock-db", default=DEFAULT_MOCK_PATH, help=f"Path to SQLite mock DB (default: {DEFAULT_MOCK_PATH})")
    parser.add_argument("--db",      default=None,            help="DuckDB path (default: data/perf_monitor.duckdb)")
    args = parser.parse_args()

    duckdb_conn = get_connection(args.db) if args.db else get_connection()

    # Source selection: use SQLite mock unless Oracle env vars are set and --mock not forced
    oracle_dsn = None
    if not args.mock:
        import os
        oracle_user = os.getenv("ORACLE_USER")
        oracle_pass = os.getenv("ORACLE_PASSWORD")
        oracle_dsn  = os.getenv("ORACLE_DSN")

    if oracle_dsn and not args.mock:
        print(f"Source: Oracle ({oracle_dsn})")
        source = OracleSource(oracle_user, oracle_pass, oracle_dsn)
    else:
        print(f"Source: SQLite mock ({args.mock_db})")
        source = SqliteSource(args.mock_db)

    result = run_import(source, duckdb_conn, days_limit=args.days, dry_run=args.dry_run)
    duckdb_conn.close()
    sys.exit(0 if result["rows"] >= 0 else 1)
