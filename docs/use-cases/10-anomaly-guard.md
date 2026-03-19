# UC-10 — The Anomaly Guard

## Problem Statement

Set a statistical baseline for "normal" behavior for every feature. Flag any day where event counts or durations deviate significantly from the norm. This serves as an early warning system for both infrastructure failures (sudden duration spikes) and unusual user activity (sudden count spikes or drops).

---

## Input Data

- `feature_daily_stats` (derived, materialized nightly):
  - `stat_date`, `feature`, `feature_type`, `event_count`, `avg_ms`, `p95_ms`
- `feature_baselines`:
  - `mean_daily_count`, `std_daily_count`, `mean_avg_ms`, `std_avg_ms`

---

## Algorithm

### Step 1 — Load baselines and today's stats

The baseline is the rolling 8-week mean and standard deviation per feature, already stored in `feature_baselines`. Compare today's values against these:

```sql
SELECT
    ds.stat_date,
    ds.feature,
    ds.feature_type,
    ds.event_count,
    ds.avg_ms,
    ds.p95_ms,
    fb.mean_daily_count,
    fb.std_daily_count,
    fb.mean_avg_ms,
    fb.std_avg_ms,
    -- Z-scores
    (ds.event_count - fb.mean_daily_count) / NULLIF(fb.std_daily_count, 0)  AS z_count,
    (ds.avg_ms - fb.mean_avg_ms) / NULLIF(fb.std_avg_ms, 0)                AS z_avg_ms,
    -- Alert classification
    CASE
        WHEN ABS((ds.event_count - fb.mean_daily_count) / NULLIF(fb.std_daily_count, 0)) > :z_threshold THEN true
        WHEN ABS((ds.avg_ms - fb.mean_avg_ms) / NULLIF(fb.std_avg_ms, 0)) > :z_threshold THEN true
        ELSE false
    END AS is_anomaly
FROM feature_daily_stats ds
JOIN feature_baselines fb
  ON ds.feature = fb.feature
  AND fb.baseline_date = (
      SELECT MAX(baseline_date) FROM feature_baselines WHERE feature = ds.feature
  )
WHERE ds.stat_date = :date
ORDER BY ABS(z_count) + ABS(z_avg_ms) DESC
```

### Step 2 — Historical anomaly scan

For trend awareness, scan the last 30 days for anomalies:

```sql
-- Same as above but with stat_date BETWEEN :start_date AND :end_date
-- Returns all (date, feature) pairs that crossed the z_threshold
```

### Step 3 — Alert classification

Anomalies are classified by type and direction:

```python
def classify_anomaly(z_count, z_avg_ms, threshold):
    alerts = []
    if z_count > threshold:
        alerts.append({ "type": "COUNT_SPIKE", "severity": "warning" if z_count < threshold*1.5 else "critical" })
    if z_count < -threshold:
        alerts.append({ "type": "COUNT_DROP", "severity": "warning" })
    if z_avg_ms > threshold:
        alerts.append({ "type": "PERF_REGRESSION", "severity": "warning" if z_avg_ms < threshold*1.5 else "critical" })
    return alerts
```

Four alert types:
- `COUNT_SPIKE`: feature used far more than usual → unusual user activity or automated script
- `COUNT_DROP`: feature used far less than usual → possible access failure or feature broken
- `PERF_REGRESSION`: feature is significantly slower → code regression or infrastructure issue
- `PERF_IMPROVEMENT`: feature is significantly faster → useful to confirm an optimization worked

---

## API Endpoint

```
GET /api/analytics/anomaly-guard
  ?date=2026-03-19           # target date (default: today)
  &z_threshold=3.0           # number of std devs to flag (default 3)
  &feature_type=             # optional filter
  &lookback_days=30          # historical scan window
```

### Response
```json
{
  "meta": {
    "date": "2026-03-19",
    "z_threshold": 3.0,
    "total_features_checked": 48,
    "anomalies_today": 2,
    "anomalies_last_30_days": 7
  },
  "today": [
    {
      "feature": "Refresh Blotter",
      "feature_type": "BLOTTER",
      "event_count": 4820,
      "avg_ms": 9200,
      "mean_daily_count": 3100,
      "mean_avg_ms": 3500,
      "z_count": 1.2,
      "z_avg_ms": 4.8,
      "is_anomaly": true,
      "alerts": [
        { "type": "PERF_REGRESSION", "severity": "critical", "detail": "avg_ms is 4.8σ above baseline" }
      ]
    }
  ],
  "history": [
    {
      "stat_date": "2026-03-10",
      "feature": "Search Contract",
      "z_count": 3.7,
      "z_avg_ms": 0.4,
      "alerts": [{ "type": "COUNT_SPIKE", "severity": "warning" }]
    }
  ],
  "scatter_data": [
    { "feature": "Refresh Blotter", "z_count": 1.2, "z_avg_ms": 4.8, "is_anomaly": true },
    { "feature": "Search Contract", "z_count": 0.3, "z_avg_ms": -0.1, "is_anomaly": false }
  ]
}
```

---

## Frontend Visualization

**Page: `AnomalyGuard.tsx`**

**Controls**:
- Date picker (default: today)
- Z-threshold slider (1.5 to 5.0, default 3.0) — live updates the scatter plot
- Feature type filter

**Panels**:
1. **Z-score scatter plot** (primary): X-axis = `z_count`, Y-axis = `z_avg_ms`. Each dot = one feature for today. Quadrant lines at ±`z_threshold`. Color: red = anomaly, grey = normal. Click a dot to drill in.
   - Top-right quadrant: too many events AND too slow → overloaded feature
   - Bottom-right: too many events but fast → possible load test or bot activity
   - Top-left: few events but slow → potential access failure with retry overhead

2. **Alert table** (below scatter): features that crossed the threshold today, sorted by severity (critical first). Shows: feature, alert type, z-scores, actual vs baseline values.

3. **Historical anomaly timeline** (bottom): calendar-style strip (last 30 days). Each day has a colored dot if anomalies were detected (grey=none, yellow=warning, red=critical). Click a day to replace the scatter with that day's data.

ECharts `ScatterChart` (primary) + custom calendar strip.

---

## Inputs to Mock

- **2026-03-14**: `Refresh Blotter` avg_ms = 9,200 (baseline 3,500 → z = +4.8) — critical performance regression
- **2026-03-10**: `Search Contract` event_count = 8,400 (baseline 3,100 → z = +3.7) — count spike anomaly
- All other days: features within 2σ of their baselines
- One `PERF_IMPROVEMENT` case: `Run P&L Report` on 2026-02-20, avg_ms drops from 15,000 to 7,000 (post-optimization deployment), z = -4.2
