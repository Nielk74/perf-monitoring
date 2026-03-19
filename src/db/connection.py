"""
src/db/connection.py

DuckDB connection factory with automatic migration runner.
Tracks applied migrations in a schema_migrations table.
"""

import os
from pathlib import Path

import duckdb

MIGRATIONS_DIR = Path(__file__).parent / "migrations"
DEFAULT_DB_PATH = "data/perf_monitor.duckdb"


def get_connection(db_path: str = DEFAULT_DB_PATH) -> duckdb.DuckDBPyConnection:
    """
    Open (or create) the DuckDB database and run any pending migrations.
    Returns a connected DuckDBPyConnection.
    """
    os.makedirs(os.path.dirname(db_path) if os.path.dirname(db_path) else ".", exist_ok=True)
    conn = duckdb.connect(db_path)
    _run_migrations(conn)
    return conn


def _run_migrations(conn: duckdb.DuckDBPyConnection) -> None:
    conn.execute("""
        CREATE TABLE IF NOT EXISTS schema_migrations (
            version     INTEGER PRIMARY KEY,
            applied_at  TIMESTAMP DEFAULT now()
        )
    """)

    applied = {row[0] for row in conn.execute("SELECT version FROM schema_migrations").fetchall()}

    migration_files = sorted(MIGRATIONS_DIR.glob("*.sql"))
    for path in migration_files:
        version = int(path.stem.split("_")[0])
        if version in applied:
            continue
        sql = path.read_text(encoding="utf-8")
        # DuckDB doesn't support multi-statement execute directly for all dialects,
        # so split on semicolons and run each statement individually.
        for statement in _split_statements(sql):
            conn.execute(statement)
        conn.execute("INSERT INTO schema_migrations (version) VALUES (?)", [version])
        print(f"  [migration] applied {path.name}")


def _split_statements(sql: str) -> list[str]:
    """Split a SQL file into individual statements, skipping blank/comment-only ones."""
    statements = []
    for raw in sql.split(";"):
        stmt = raw.strip()
        # Strip comment lines to check if there's real SQL
        lines = [l for l in stmt.splitlines() if not l.strip().startswith("--")]
        body = " ".join(lines).strip()
        if body:
            statements.append(stmt)
    return statements
