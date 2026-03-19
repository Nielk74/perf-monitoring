# 01 — Architecture Overview

## System Diagram

```
┌─────────────────────────────────────────────────────────────────────┐
│  DATA SOURCES                                                       │
│                                                                     │
│  ┌──────────────────┐    ┌──────────────────┐                      │
│  │  Oracle DB       │    │  Git Repository  │                      │
│  │  STAR_ACTION_AUDIT│    │  (app source)    │                      │
│  └────────┬─────────┘    └────────┬─────────┘                      │
│           │                       │                                 │
└───────────┼───────────────────────┼─────────────────────────────────┘
            │                       │
            ▼                       ▼
┌─────────────────────────────────────────────────────────────────────┐
│  INGESTION LAYER  (src/ingestion/)                                  │
│                                                                     │
│  ┌──────────────────┐    ┌──────────────────┐                      │
│  │  oracle_importer │    │  git_importer    │                      │
│  │  • Incremental   │    │  • Commits       │                      │
│  │  • 12-month max  │    │  • File changes  │                      │
│  └────────┬─────────┘    └────────┬─────────┘                      │
│           │                       │                                 │
│           └──────────┬────────────┘                                │
│                      ▼                                              │
│           ┌──────────────────┐                                     │
│           │   scheduler.py   │  APScheduler cron jobs              │
│           └──────────────────┘                                     │
└──────────────────────┬──────────────────────────────────────────────┘
                       │
                       ▼
┌─────────────────────────────────────────────────────────────────────┐
│  STORAGE  (data/perf_monitor.duckdb)                                │
│                                                                     │
│  Raw tables:      audit_events, commits, commit_files              │
│  Derived tables:  feature_baselines, user_sessions, ...            │
└──────────────────────┬──────────────────────────────────────────────┘
                       │
                       ▼
┌─────────────────────────────────────────────────────────────────────┐
│  ANALYTICS ENGINE  (src/analytics/)                                 │
│                                                                     │
│  One Python module per use case. Each module:                       │
│  • Runs DuckDB SQL queries directly                                 │
│  • Returns structured Python dicts / lists for the API             │
│  • Stateless — no caching, DuckDB is the single truth source       │
└──────────────────────┬──────────────────────────────────────────────┘
                       │
                       ▼
┌─────────────────────────────────────────────────────────────────────┐
│  WEB API  (src/api/)                                                │
│                                                                     │
│  FastAPI — JSON REST endpoints                                      │
│  • GET /api/ingestion/status                                        │
│  • POST /api/ingestion/run                                          │
│  • GET /api/uc/{use-case-id}?params=...                             │
└──────────────────────┬──────────────────────────────────────────────┘
                       │
                       ▼
┌─────────────────────────────────────────────────────────────────────┐
│  FRONTEND  (frontend/)                                              │
│                                                                     │
│  React SPA + Apache ECharts                                         │
│  • One page per use case                                            │
│  • Shared filter bar: date range, feature type, user, workstation  │
└─────────────────────────────────────────────────────────────────────┘
```

---

## Technology Choices

| Layer | Technology | Rationale |
|-------|-----------|-----------|
| Storage | **DuckDB** | In-process, columnar, OLAP-optimized. No server to operate. Handles hundreds of millions of rows comfortably. Native time-series aggregations. |
| Oracle connector | **python-oracledb** (thin mode) | No Oracle client install needed. Official Cx_Oracle successor. |
| Git parsing | **GitPython** | Pure Python, covers all needed metadata (commits, diffs, tags). |
| Scheduler | **APScheduler** | Lightweight cron-style scheduler, runs inside the FastAPI process or as a standalone daemon. |
| API server | **FastAPI** | Async, auto-generated OpenAPI docs, minimal boilerplate. |
| Analytics | **DuckDB Python API** | SQL runs directly on DuckDB file — no ORM, no pandas overhead for aggregations. |
| Frontend framework | **React 18** | Component reuse across 10 use-case pages. |
| Charting | **Apache ECharts** + **echarts-for-react** | Handles Gantt, Sankey, heatmaps, scatter, and time-series natively. |
| Data format | **JSON** over REST | Simple, stateless, works with any future frontend. |

---

## Key Design Decisions

### Single DuckDB file
All data lives in `data/perf_monitor.duckdb`. This is a deliberate choice: the platform is a read-heavy analytics tool, not a transactional system. DuckDB's columnar storage gives sub-second query times on millions of rows without any infrastructure.

### No ORM
Analytics queries are complex window functions, CTEs, and aggregations. SQL is the right language for this. All queries are written as plain SQL strings executed through `duckdb.connect()`.

### Ingestion is append-only
`audit_events` rows are never updated. The importer tracks a high-water mark (`last_imported_at`) and only pulls new rows. This makes ingestion safe to re-run and idempotent.

### Derived tables are materialized on a schedule
`feature_baselines`, `user_sessions`, and `feature_sequences` are expensive to compute on every request. They are rebuilt nightly by the scheduler, not on-demand.

### Mock-first development
`src/mock/generator.py` generates a full synthetic dataset into DuckDB. The Oracle importer and Git importer are wired in only when real sources are available. The entire analytics stack and frontend work identically on mock data.

---

## Process Model

**Development mode:**
```
python src/mock/generator.py          # populate DuckDB with synthetic data
uvicorn src.api.main:app --reload     # API on :8000
npm run dev  (frontend/)              # Vite dev server on :5173, proxy to :8000
```

**Production mode:**
```
python src/ingestion/scheduler.py     # runs Oracle + Git imports on cron
uvicorn src.api.main:app              # API on :8000
nginx -> frontend/dist/               # static SPA + reverse proxy to API
```

---

## Configuration

`config/settings.yaml`:
```yaml
duckdb:
  path: data/perf_monitor.duckdb

oracle:
  host: ora-server.internal
  port: 1521
  service_name: STAR
  user: readonly_user
  password: "${ORACLE_PASSWORD}"
  max_months: 12

git:
  repos:
    - path: /path/to/app-repo
      name: star-app
  max_commits: 5000

scheduler:
  oracle_cron: "0 2 * * *"     # nightly 2am
  git_cron: "*/30 * * * *"     # every 30 minutes
  materialize_cron: "0 3 * * *" # nightly 3am

api:
  host: 0.0.0.0
  port: 8000
```
