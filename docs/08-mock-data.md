# 08 — Mock Data Generator

## Purpose

Generate a complete, realistic synthetic dataset in DuckDB that exercises every use case. Runs once (or on reset). All analytics, API, and frontend work identically on mock data.

---

## Module: `src/mock/generator.py`

### Usage

```bash
# Generate full mock dataset (12 months, ~2M events)
python -m src.mock.generator

# Smaller dataset for fast iteration
python -m src.mock.generator --scale small   # ~100k events, 3 months

# Reset and regenerate
python -m src.mock.generator --reset
```

---

## Synthetic Data Parameters

```python
SCALE_PROFILES = {
    "small":  { "users": 20,  "workstations": 30,  "days": 90,  "events_per_day": 500 },
    "medium": { "users": 80,  "workstations": 120, "days": 180, "events_per_day": 5000 },
    "large":  { "users": 200, "workstations": 350, "days": 365, "events_per_day": 15000 },
}
```

---

## Feature Taxonomy

Mirrors the real Oracle `FEATURE_TYPE` + `FEATURE` structure. Defined as a weighted distribution:

```python
FEATURES = [
    # (feature_type, feature, weight, has_duration, base_duration_ms, stddev_ms)
    ("SYSTEM",  "Star Startup",                1,  True,  120000, 30000),
    ("SEARCH",  "Search Contract",            15,  True,    800,   400),
    ("SEARCH",  "Search Portfolio",           10,  True,   1200,   600),
    ("BLOTTER", "Open Blotter",                8,  True,   2500,  1200),
    ("BLOTTER", "Refresh Blotter",            12,  True,   3500,  2000),
    ("TOOLS",   "Copy Contract No",           20, False,   None,  None),
    ("TOOLS",   "Export to Excel",             6,  True,   4500,  1500),
    ("REPORT",  "Run P&L Report",              5,  True,  15000,  5000),
    ("REPORT",  "Run Position Report",         4,  True,  12000,  4000),
    ("STATIC",  "Open Static Data",            7,  True,   2000,   800),
    ("STATIC",  "Edit Counterparty",           3,  True,   1500,   500),
    ("PTFLABEL","ShowPortfolioLabel Permission",2, False,   None,  None),
    # Low-usage / zombie features
    ("REPORT",  "Legacy Audit Trail Report",   1,  True,  45000, 10000),
    ("TOOLS",   "Old Batch Processor",         1,  True,  60000, 20000),
    ("STATIC",  "Archive Data Viewer",         1,  True,   3000,  1000),
]
```

Zombie features (`Legacy Audit Trail Report`, `Old Batch Processor`, `Archive Data Viewer`) are used only in the first 6 months of the dataset, then drop to zero — enabling the **Feature Sunset** use case.

---

## User & Workstation Generation

```python
OFFICES = {
    "LON": { "users": 40, "workstations": 60, "tz": "Europe/London" },
    "NYC": { "users": 25, "workstations": 35, "tz": "America/New_York" },
    "SIN": { "users": 15, "workstations": 20, "tz": "Asia/Singapore" },
}

# User format: 2-char prefix + 2-digit number (e.g., MS01, JD03)
# Workstation format: {OFFICE}W{8-digit-number} (e.g., LONW00081256)
# Support users: prefixed "supp_" (5 support accounts across all offices)
```

---

## Use-Case-Specific Patterns

### UC-01 Silent Degrader
- 3 features have a +5–8% weekly drift in `duration_ms` starting ~10 weeks before "today"
- Implemented by multiplying the base duration by `1.06 ^ week_index` for those features

### UC-02 Blast Radius
- 2 commits have an associated performance spike: events in the 48h window after `deployed_at` show 2× median duration for specific features
- These commits are tagged `v2.12.0` and `v2.14.0`

### UC-03 Environmental Fingerprint
- LON office workstations have a +40% startup time (simulates a network issue)
- SIN office workstations have normal performance

### UC-04 Impersonation Audit
- 5 support users perform ~50 impersonated sessions per month
- Impersonated sessions show 15% lower feature durations (support staff are faster/more efficient)

### UC-05 Feature Sunset
- 3 features have zero events in the last 6 months
- 2 features have <5 distinct users in the last 12 months

### UC-06 Friction Heatmap
- `Refresh Blotter` and `Run P&L Report` have high `total_wait_ms` due to high frequency × high duration

### UC-07 Workflow Discovery
- Common sequence: Search Contract → Open Blotter → Export to Excel (60% of sessions containing Search)
- Another sequence: Open Static Data → Edit Counterparty (80% co-occurrence)

### UC-08 Adoption Velocity
- Feature `New Blotter View` was introduced 90 days ago (first appearance in mock data)
- Adoption follows an S-curve: slow first 2 weeks, rapid growth weeks 3–6, plateau

### UC-09 Peak Pressure
- Month-end dates (last 2 business days of month) have 3× normal event volume
- NYC office is busiest 14:00–16:00 UTC (market hours)
- SIN office is busiest 01:00–03:00 UTC

### UC-10 Anomaly Guard
- 2 recent dates have anomalous spikes: one in event count (+4σ), one in duration (+5σ)
- These are injected on specific dates so the Anomaly Guard page shows clear alerts

---

## Generation Algorithm

```python
import duckdb, random, datetime
from faker import Faker  # for realistic names

def generate(scale: str = "medium"):
    conn = duckdb.connect("data/perf_monitor.duckdb")
    run_migrations(conn)

    params = SCALE_PROFILES[scale]
    users, workstations = generate_users_and_workstations(params)
    commits = generate_commits(params)

    events = []
    current_date = datetime.date.today() - datetime.timedelta(days=params["days"])
    end_date = datetime.date.today()

    while current_date <= end_date:
        daily_events = generate_day(current_date, users, workstations, params, commits)
        events.extend(daily_events)
        if len(events) >= 50_000:
            flush_events(conn, events)
            events = []
        current_date += datetime.timedelta(days=1)

    flush_events(conn, events)
    materialize_all(conn)   # rebuild derived tables
    conn.close()
```

---

## Output Verification

After generation, the script prints a summary:

```
Mock data generation complete
  audit_events:        1,824,310 rows
  commits:             412 rows
  commit_files:        6,180 rows
  user_sessions:       48,230 rows (materialized)
  feature_sequences:   187 distinct transitions (materialized)

Use-case coverage check:
  [OK] UC-01 Silent Degrader:         3 features with slope > 5%/week
  [OK] UC-02 Blast Radius:            2 problematic commits identified
  [OK] UC-03 Environmental Fingerprint: LON startup +42% vs global
  [OK] UC-04 Impersonation Audit:     312 impersonated sessions
  [OK] UC-05 Feature Sunset:          3 zombie features (0 events in last 6m)
  [OK] UC-06 Friction Heatmap:        Top feature: Refresh Blotter (142B ms total)
  [OK] UC-07 Workflow Discovery:      Search→Blotter→Export: 68% of sessions
  [OK] UC-08 Adoption Velocity:       New Blotter View: S-curve over 90 days
  [OK] UC-09 Peak Pressure:           Month-end 3.1× baseline confirmed
  [OK] UC-10 Anomaly Guard:           2 anomalous dates detected (z > 3)
```
