# Performance Monitoring

Performance analytics platform for audit event data. Ingests Oracle audit logs and Git commit history into DuckDB, materializes 10 analytics use-cases, and exposes them through a FastAPI REST API and a React/ECharts dashboard.

---

## Stack

| Layer | Technology |
|---|---|
| Storage | DuckDB |
| Backend | Python 3.11+, FastAPI, Uvicorn |
| Frontend | React 18, TypeScript, Vite, TailwindCSS, ECharts |
| Data sources | Oracle (audit events), Git repos |

---

## Quick start

Run these commands in order from the repo root. Everything below assumes a fresh clone.

### 1. Install dependencies

```bash
pip install -r requirements.txt
```

```bash
cd frontend && npm install && cd ..
```

### 2. Generate mock data

Creates `data/mock_oracle.db` and `data/mock_star_repo/` (the `data/` folder is created automatically):

```bash
python mock_oracle.py
python mock_git_repo.py
```

### 3. Ingest data into DuckDB

```bash
python -m src.ingestion.oracle_importer
python -m src.ingestion.git_importer
```

### 4. Materialize analytics tables

```bash
python -m src.analytics.materializer
```

### 5. Start the API

```bash
uvicorn src.api.main:app --reload --port 8001
```

Check it is up: `curl http://localhost:8001/health` should return `{"status":"ok"}`.

### 6. Start the frontend (separate terminal)

```bash
cd frontend && npm run dev
```

Open **http://localhost:5173** in your browser.

---

## Re-generating mock data

If you need to reset and regenerate:

```bash
python mock_oracle.py --reset
python mock_git_repo.py --reset
```

### Scaling the dataset

`mock_oracle.py` ships with three built-in profiles and individual overrides:

```bash
python mock_oracle.py --scale small               # 3 months, ~100k events
python mock_oracle.py --scale medium              # 6 months, ~900k events (default)
python mock_oracle.py --scale large               # 12 months, ~5.5M events

# Fine-grained overrides (applied on top of the chosen profile):
python mock_oracle.py --users 500
python mock_oracle.py --days 60
python mock_oracle.py --events-per-day 20000
python mock_oracle.py --scale large --users 1000 --events-per-day 50000
```

`mock_git_repo.py` accepts history length and commit density overrides:

```bash
python mock_git_repo.py --days 180               # shorter history (default: 365)
python mock_git_repo.py --commits-per-day 5      # denser commit log (default: 2)
```

Always pass `--reset` to replace an existing dataset.

Then repeat steps 3 and 4 above.

---

## Prerequisites

- Python 3.11+
- Node.js 18+

---

## Configuration

Edit `config/settings.yaml` to point to your DuckDB file, Oracle mock DB, and Git repositories:

```yaml
duckdb:
  path: data/perf_monitor.duckdb

oracle:
  mock_db: data/mock_oracle.db

git:
  repos:
    - path: data/mock_repo
      name: my-app
```

For a real Oracle connection, uncomment `python-oracledb` in `requirements.txt` and set the following environment variables:

```
ORACLE_USER=...
ORACLE_PASSWORD=...
ORACLE_DSN=...
```

---

## Project structure

```
.
├── config/             # settings.yaml
├── data/               # DuckDB + mock data (git-ignored)
├── frontend/           # React/Vite app
│   └── src/
│       ├── components/
│       ├── pages/      # 11 analytics pages
│       └── api/        # API client
├── src/
│   ├── analytics/      # 10 use-case modules + materializer
│   ├── api/            # FastAPI routers
│   ├── db/             # DuckDB connection
│   └── ingestion/      # Oracle + Git importers
├── mock_oracle.py
├── mock_git_repo.py
└── requirements.txt
```

---

## Analytics use-cases

| # | Name | Description |
|---|---|---|
| 01 | Silent Degrader | Features with a rising weekly latency slope |
| 02 | Blast Radius | Before/after latency impact of a deployment |
| 03 | Environmental Fingerprint | Per-office latency comparison |
| 04 | Impersonation Audit | Support session audit trail |
| 05 | Feature Sunset | Features with significant usage decline |
| 06 | Friction Heatmap | Total user wait time per feature |
| 07 | Workflow Discovery | Feature navigation flow via Sankey diagram |
| 08 | Adoption Velocity | Weekly active user S-curves for new features |
| 09 | Peak Pressure | Hour x day-of-week event volume heatmap |
| 10 | Anomaly Guard | Z-score anomaly detection vs 8-week baseline |
