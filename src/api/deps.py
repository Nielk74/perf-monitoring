"""
src/api/deps.py — FastAPI dependency injection.

DuckDB on Windows uses exclusive file locking — only one connection can be
open per process, and the Python client is not thread-safe. FastAPI runs
sync route handlers in a thread pool, so concurrent requests race on the
shared connection and produce 500 errors.

Fix: a single connection protected by a threading.Lock. Every execute()
call acquires the lock, runs the query, eagerly fetches all results into
Python memory, then releases the lock. The returned EagerCursor is a plain
in-memory object — safe to use after the lock is gone.
"""

import threading
from typing import Any

import duckdb

from src.db.connection import get_connection


class _EagerCursor:
    """In-memory cursor — safe to read after the DB lock is released."""

    def __init__(self, description: list | None, data: list[tuple]):
        self.description = description
        self._data = data

    def fetchall(self) -> list[tuple]:
        return self._data

    def fetchone(self) -> tuple | None:
        return self._data[0] if self._data else None

    def fetchmany(self, size: int = 1) -> list[tuple]:
        return self._data[:size]


class _ThreadSafeConn:
    """Single DuckDB connection serialised with a lock — thread-safe wrapper."""

    def __init__(self, raw: duckdb.DuckDBPyConnection):
        self._conn = raw
        self._lock = threading.Lock()

    def execute(self, sql: str, params: list[Any] | None = None) -> _EagerCursor:
        with self._lock:
            cur = self._conn.execute(sql, params or [])
            description = cur.description  # may be None for non-SELECT
            data = cur.fetchall()
        return _EagerCursor(description, data)

    # Passthrough so analytics modules that import the raw conn type still work
    def cursor(self) -> "_ThreadSafeConn":
        return self


_conn: _ThreadSafeConn | None = None
_init_lock = threading.Lock()


def get_db() -> _ThreadSafeConn:
    """Return the process-wide thread-safe DuckDB connection."""
    global _conn
    if _conn is None:
        with _init_lock:
            if _conn is None:
                _conn = _ThreadSafeConn(get_connection())
    return _conn
