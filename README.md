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

## Prerequisites

- Python 3.11+
- Node.js 18+

---

## Installation

### 1. Clone the repo

```bash
git clone <repo-url>
cd perf-monitoring
```

### 2. Python dependencies

```bash
pip install -r requirements.txt
```

### 3. Frontend dependencies

```bash
cd frontend
npm install
cd ..
```

---

## Setup

### Generate mock data

The project ships with generators for both Oracle audit data and a Git repository. Run them once to populate `data/`:

```bash
python mock_oracle.py
python mock_git_repo.py
```

### Ingest data into DuckDB

```bash
python -m src.ingestion.oracle_importer
python -m src.ingestion.git_importer
```

### Materialize analytics tables

```bash
python -m src.analytics.materializer
```

---

## Running

### Backend (FastAPI)

```bash
uvicorn src.api.main:app --reload --port 8000
```

### Frontend (Vite dev server)

```bash
cd frontend
npm run dev
```

The dashboard is available at **http://localhost:5173**.

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
    - path: data/mock_star_repo
      name: star-app
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
