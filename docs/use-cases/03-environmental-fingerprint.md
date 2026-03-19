# UC-03 — The Environmental Fingerprint

## Problem Statement

Determine whether a performance issue lives in the code (affects everyone) or is tied to specific hardware, network, or office infrastructure (affects a subset of workstations). A global software regression looks very different from a network bottleneck in one office.

---

## Input Data

- `audit_events` — filtered to timed events (`duration_ms IS NOT NULL`)
- `workstation_profiles` — office prefix per workstation

The office prefix is derived from the first 3 characters of `workstation` (e.g., `LON` from `LONW00081256`).

---

## Algorithm

### Step 1 — Group performance by office prefix

For a given feature and time range:

```sql
SELECT
    SUBSTR(workstation, 1, 3)                       AS office,
    COUNT(*)                                         AS event_count,
    COUNT(DISTINCT asmd_user)                        AS distinct_users,
    AVG(duration_ms)                                 AS avg_ms,
    MEDIAN(duration_ms)                              AS median_ms,
    PERCENTILE_CONT(0.25) WITHIN GROUP (ORDER BY duration_ms) AS p25_ms,
    PERCENTILE_CONT(0.75) WITHIN GROUP (ORDER BY duration_ms) AS p75_ms,
    PERCENTILE_CONT(0.95) WITHIN GROUP (ORDER BY duration_ms) AS p95_ms,
    STDDEV(duration_ms)                              AS std_ms,
    MIN(duration_ms)                                 AS min_ms,
    MAX(duration_ms)                                 AS max_ms
FROM audit_events
WHERE feature = :feature
  AND duration_ms IS NOT NULL
  AND mod_dt BETWEEN :start_date AND :end_date
GROUP BY SUBSTR(workstation, 1, 3)
HAVING COUNT(*) >= 20
```

### Step 2 — Compute global baseline

```sql
SELECT
    AVG(duration_ms)    AS global_avg_ms,
    MEDIAN(duration_ms) AS global_median_ms,
    STDDEV(duration_ms) AS global_std_ms
FROM audit_events
WHERE feature = :feature
  AND duration_ms IS NOT NULL
  AND mod_dt BETWEEN :start_date AND :end_date
```

### Step 3 — Flag anomalous offices

An office is flagged as an environmental outlier if its `avg_ms` is more than 1.5× standard deviations above the global mean, or if its `avg_ms` deviates from the global mean by more than a configurable percentage threshold (default 30%):

```python
delta_pct = (office_avg_ms - global_avg_ms) / global_avg_ms * 100
is_outlier = abs(delta_pct) > threshold_pct   # default 30%
```

### Step 4 — Workstation-level drill-down (optional)

For a flagged office, show individual workstation performance to isolate whether it's the whole office or specific machines:

```sql
SELECT
    workstation,
    COUNT(*)                AS event_count,
    AVG(duration_ms)        AS avg_ms,
    PERCENTILE_CONT(0.95) WITHIN GROUP (ORDER BY duration_ms) AS p95_ms
FROM audit_events
WHERE feature = :feature
  AND SUBSTR(workstation, 1, 3) = :office
  AND duration_ms IS NOT NULL
  AND mod_dt BETWEEN :start_date AND :end_date
GROUP BY workstation
ORDER BY avg_ms DESC
```

---

## API Endpoint

```
GET /api/analytics/environmental-fingerprint
  ?feature=Star+Startup
  &start_date=2026-01-01
  &end_date=2026-03-19
  &threshold_pct=30
  &workstation=LONW00081256   # optional: drill into one workstation
```

### Response
```json
{
  "meta": { "feature": "Star Startup", "global_avg_ms": 121000, "global_std_ms": 28000 },
  "offices": [
    {
      "office": "LON",
      "event_count": 4820,
      "distinct_users": 38,
      "avg_ms": 169400,
      "p25_ms": 130000,
      "median_ms": 162000,
      "p75_ms": 195000,
      "p95_ms": 248000,
      "delta_pct": 40.0,
      "is_outlier": true
    },
    {
      "office": "NYC",
      "event_count": 3010,
      "avg_ms": 118000,
      "delta_pct": -2.5,
      "is_outlier": false
    }
  ],
  "workstation_detail": null
}
```

---

## Frontend Visualization

**Page: `EnvironmentalFingerprint.tsx`**

**Controls**:
- Feature selector (searchable dropdown)
- Date range
- Threshold slider (% deviation to flag)

**Panels**:
1. **Box plot per office**: median, IQR, whiskers, outlier dots. Each box is one office. Global median shown as a dashed horizontal reference line. Color: red if `is_outlier`, grey otherwise.
2. **Office summary table**: avg_ms, p95_ms, delta_pct, event_count — sortable
3. **Drill-down**: click an office to see workstation-level bar chart (avg_ms per workstation, sorted descending)

ECharts `BoxplotChart` natively supports this visualization.

---

## Inputs to Mock

- LON office: `Star Startup` avg_ms = 170,000 (global avg: 120,000 → +42%)
- SIN office: `Star Startup` avg_ms = 115,000 (within normal range)
- NYC office: `Star Startup` avg_ms = 122,000 (within normal range)
- 2 specific LON workstations with even higher startup times (240,000 ms) to show workstation drill-down
