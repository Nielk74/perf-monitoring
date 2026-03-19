# 06 — Web API

## Framework: FastAPI

`src/api/main.py` is the application entry point. All routes are JSON REST. OpenAPI docs auto-generated at `/docs`.

---

## Application Structure

```
src/api/
├── main.py              # FastAPI app, startup/shutdown, CORS, router registration
├── deps.py              # Dependency injection: DB connection, config
└── routes/
    ├── ingestion.py     # Trigger/status ingestion jobs
    ├── analytics.py     # All 10 use-case endpoints
    └── metadata.py      # Feature list, user list, date range info
```

---

## `main.py`

```python
from fastapi import FastAPI
from fastapi.middleware.cors import CORSMiddleware
from src.api.routes import ingestion, analytics, metadata

app = FastAPI(title="Perf Monitoring API", version="1.0")

app.add_middleware(CORSMiddleware, allow_origins=["*"], allow_methods=["*"], allow_headers=["*"])

app.include_router(ingestion.router, prefix="/api/ingestion")
app.include_router(analytics.router, prefix="/api/analytics")
app.include_router(metadata.router, prefix="/api/metadata")
```

---

## Dependency Injection: `deps.py`

```python
import duckdb
from functools import lru_cache
from src.config import settings

@lru_cache(maxsize=1)
def get_db() -> duckdb.DuckDBPyConnection:
    return duckdb.connect(str(settings.duckdb.path), read_only=True)
```

Injected into routes with `Depends(get_db)`.

---

## Routes: Ingestion (`/api/ingestion`)

| Method | Path | Description |
|--------|------|-------------|
| GET | `/status` | Last run time, row counts, next scheduled run |
| POST | `/run/oracle` | Trigger Oracle import manually |
| POST | `/run/git` | Trigger Git import manually |
| POST | `/run/materialize` | Rebuild all derived tables |
| POST | `/deployment` | Register a deployment event `{commit_hash, deployed_at}` |

### `GET /api/ingestion/status` Response
```json
{
  "audit_events": { "row_count": 4821033, "max_mod_dt": "2026-03-19T14:55:34Z", "last_import": "2026-03-19T02:01:15Z" },
  "commits": { "row_count": 1243, "last_committed_at": "2026-03-18T17:22:00Z" },
  "derived_tables": { "last_materialized": "2026-03-19T03:00:44Z" }
}
```

---

## Routes: Analytics (`/api/analytics`)

All endpoints accept common query params where applicable:
- `start_date` / `end_date` (ISO date strings)
- `feature_type` (optional filter)
- `office` (optional workstation prefix filter)

### UC-01 Silent Degrader
```
GET /api/analytics/silent-degrader
  ?weeks=8
  &slope_threshold_pct=3.0
  &feature_type=SEARCH
```

### UC-02 Blast Radius
```
GET /api/analytics/blast-radius
  ?commit_hash=abc123def456
```
or
```
GET /api/analytics/blast-radius
  ?tag=v2.14.0
```

### UC-03 Environmental Fingerprint
```
GET /api/analytics/environmental-fingerprint
  ?feature=Star+Startup
  &start_date=2026-01-01
  &end_date=2026-03-19
```

### UC-04 Impersonation Audit
```
GET /api/analytics/impersonation-audit
  ?asmd_user=MS01
  &start_date=2026-03-01
  &end_date=2026-03-19
```

### UC-05 Feature Sunset
```
GET /api/analytics/feature-sunset
  ?months=12
  &min_users_threshold=0
```

### UC-06 Friction Heatmap
```
GET /api/analytics/friction-heatmap
  ?start_date=2026-01-01
  &end_date=2026-03-19
  &top_n=30
```

### UC-07 Workflow Discovery
```
GET /api/analytics/workflow-discovery/transitions
  ?min_count=50

GET /api/analytics/workflow-discovery/gantt
  ?asmd_user=MS01
  &date=2026-03-12
```

### UC-08 Adoption Velocity
```
GET /api/analytics/adoption-velocity
  ?feature=New+Blotter+View
  &launch_date=2026-01-15
  &days=90
```

### UC-09 Peak Pressure
```
GET /api/analytics/peak-pressure
  ?granularity=hour        # hour | day | week
  &start_date=2026-01-01
  &end_date=2026-03-19
```

### UC-10 Anomaly Guard
```
GET /api/analytics/anomaly-guard
  ?z_threshold=3.0
  &date=2026-03-19
```

---

## Routes: Metadata (`/api/metadata`)

| Method | Path | Description |
|--------|------|-------------|
| GET | `/features` | All distinct features + types |
| GET | `/users` | All distinct users (asmd_user) |
| GET | `/workstations` | All distinct workstations with office prefix |
| GET | `/offices` | Distinct office prefixes derived from workstations |
| GET | `/date-range` | Min and Max `mod_dt` in `audit_events` |
| GET | `/commits` | Recent commits list for Blast Radius selector |

### `GET /api/metadata/features` Response
```json
[
  { "feature_type": "SEARCH", "feature": "Search Contract", "event_count": 142310 },
  { "feature_type": "TOOLS", "feature": "Copy Contract No", "event_count": 89204 },
  ...
]
```

---

## Error Responses

All errors follow RFC 7807:
```json
{
  "status": 400,
  "title": "Bad Request",
  "detail": "start_date must be before end_date"
}
```

---

## Running the API

```bash
# Development
uvicorn src.api.main:app --reload --port 8000

# Production
uvicorn src.api.main:app --host 0.0.0.0 --port 8000 --workers 4
```

OpenAPI docs: `http://localhost:8000/docs`
