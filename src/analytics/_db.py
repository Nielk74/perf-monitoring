"""Shared DuckDB connection for the analytics layer."""

from functools import lru_cache

import duckdb

from src.db.connection import get_connection


@lru_cache(maxsize=1)
def get_ro_connection() -> duckdb.DuckDBPyConnection:
    """Single shared connection (write-capable so ingestion can reuse it)."""
    return get_connection()


def rows(cursor) -> list[dict]:
    """Convert a DuckDB cursor result to a list of dicts."""
    cols = [d[0] for d in cursor.description]
    return [dict(zip(cols, row)) for row in cursor.fetchall()]
