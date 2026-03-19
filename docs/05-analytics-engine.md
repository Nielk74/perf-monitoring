# 05 — Analytics Engine

## Design Philosophy

- Each use case is a **single Python module** in `src/analytics/`
- All computation runs as **DuckDB SQL** — no pandas, no Polars in the hot path
- Modules are **stateless functions** — they accept a DuckDB connection + parameters, run SQL, and return structured Python dicts
- The API layer calls these functions directly — no intermediate service layer

---

## Module Structure

```
src/analytics/
├── __init__.py
├── _db.py                     # shared DuckDB connection helper
├── silent_degrader.py         # UC-01
├── blast_radius.py            # UC-02
├── environmental_fingerprint.py # UC-03
├── impersonation_audit.py     # UC-04
├── feature_sunset.py          # UC-05
├── friction_heatmap.py        # UC-06
├── workflow_discovery.py      # UC-07
├── adoption_velocity.py       # UC-08
├── peak_pressure.py           # UC-09
└── anomaly_guard.py           # UC-10
```

---

## Shared DB Helper: `src/analytics/_db.py`

```python
import duckdb
from functools import lru_cache
from pathlib import Path
from src.config import settings

@lru_cache(maxsize=1)
def get_connection() -> duckdb.DuckDBPyConnection:
    """Return a single shared read connection to the DuckDB file."""
    return duckdb.connect(str(settings.duckdb.path), read_only=True)
```

> **Note:** Ingestion uses a separate writable connection. Analytics uses a read-only connection to avoid write conflicts.

---

## Standard Module Interface

Every analytics module exports one or more functions following this signature:

```python
def get_<use_case>(
    conn: duckdb.DuckDBPyConnection,
    **params
) -> dict:
    """
    Returns:
        {
            "meta": { ...query metadata... },
            "data": [ ...rows as dicts... ]
        }
    """
```

Params are passed from the API query string. Each module documents its accepted parameters.

---

## Common Query Patterns

### Time Window Filter
Most endpoints accept `start_date` / `end_date`:

```sql
WHERE mod_dt BETWEEN :start_date AND :end_date
```

Default: last 90 days. Max: 12 months.

### Feature Type Filter
```sql
AND (:feature_type IS NULL OR feature_type = :feature_type)
```

### Workstation Office Filter (prefix-based)
```sql
AND (:office IS NULL OR workstation LIKE :office || '%')
```

### Percentile Aggregation (DuckDB syntax)
```sql
PERCENTILE_CONT(0.95) WITHIN GROUP (ORDER BY duration_ms) AS p95_ms
```

### Linear Regression (Trend Slope)
```sql
REGR_SLOPE(avg_ms, epoch_week) AS slope_ms_per_week
```

DuckDB supports `REGR_SLOPE` natively as a window/aggregate function.

### Session Reconstruction (Gap = 30 min)
```sql
WITH gaps AS (
    SELECT
        asmd_user,
        workstation,
        mod_dt,
        LAG(mod_dt) OVER (PARTITION BY asmd_user, workstation ORDER BY mod_dt) AS prev_dt,
        EPOCH(mod_dt) - EPOCH(LAG(mod_dt) OVER (PARTITION BY asmd_user, workstation ORDER BY mod_dt)) > 1800
            AS is_new_session
    FROM audit_events
),
sessions AS (
    SELECT
        *,
        SUM(is_new_session::INT) OVER (PARTITION BY asmd_user, workstation ORDER BY mod_dt) AS session_id
    FROM gaps
)
```

### Z-Score Anomaly Detection
```sql
(current_value - mean_value) / NULLIF(std_value, 0) AS z_score
```

---

## Materialization Jobs

Heavy derived tables are refreshed by `src/analytics/materializer.py`, called by the scheduler nightly:

```python
def materialize_all(conn: duckdb.DuckDBPyConnection):
    materialize_feature_daily_stats(conn)
    materialize_feature_baselines(conn)
    materialize_user_sessions(conn)
    materialize_feature_sequences(conn)
    materialize_workstation_profiles(conn)
```

Each function truncates and rebuilds its target table. Since DuckDB is a local file, this is safe and fast (typically completes in under 60 seconds for 12 months of data).

---

## Performance Expectations

DuckDB query times on a single developer machine (12 months, ~5M rows):

| Query type | Expected time |
|------------|--------------|
| Daily aggregation (feature_daily_stats) | < 2s |
| Rolling window regression (8 weeks) | < 1s |
| Session reconstruction (user_sessions) | 5–15s (materialized nightly) |
| Feature sequence transition matrix | < 1s on materialized table |
| Z-score anomaly scan | < 500ms |
| Full Gantt session export (1 user, 1 day) | < 200ms |

All interactive API endpoints query from **materialized derived tables**, not raw `audit_events`, keeping response times under 500ms.

---

## Adding a New Use Case

1. Create `src/analytics/my_new_use_case.py` with a `get_my_new_use_case(conn, **params)` function
2. Add a route in `src/api/routes/analytics.py`
3. Add a page in `frontend/src/pages/`
4. Document in `docs/use-cases/`

No schema changes needed unless the use case requires a new derived table.
