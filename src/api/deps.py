"""
src/api/deps.py — FastAPI dependency injection.

Analytics routes get a shared read-only DuckDB connection.
Ingestion routes open their own short-lived write connections.
"""

from functools import lru_cache

import duckdb

from src.db.connection import get_connection


@lru_cache(maxsize=1)
def get_db() -> duckdb.DuckDBPyConnection:
    """Shared DuckDB connection for all routes (analytics, metadata, ingestion).

    Uses a single write-capable connection so ingestion background tasks can
    share it without hitting DuckDB's "different configuration" conflict.
    """
    return get_connection()
