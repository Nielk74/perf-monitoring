"""Shared read-only DuckDB connection for the analytics layer."""

from functools import lru_cache

import duckdb

from src.config import settings


@lru_cache(maxsize=1)
def get_ro_connection() -> duckdb.DuckDBPyConnection:
    """Single shared read-only connection. Ingestion uses a separate write connection."""
    return duckdb.connect(settings.duckdb.path, read_only=True)


def rows(cursor) -> list[dict]:
    """Convert a DuckDB cursor result to a list of dicts."""
    cols = [d[0] for d in cursor.description]
    return [dict(zip(cols, row)) for row in cursor.fetchall()]
