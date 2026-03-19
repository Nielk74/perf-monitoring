# UC-01 — The Silent Degrader

## Problem Statement

Detect features whose performance is slowly getting worse over time — not "broken" but degrading 5% per week. These regressions are invisible in incident-based monitoring but compound into serious user friction and often signal resource leaks or inefficient code paths.

---

## Input Data

From `feature_daily_stats` (derived, materialized nightly):
- `stat_date`, `feature`, `feature_type`, `p95_ms`, `avg_ms`, `event_count`

From `feature_baselines`:
- `weekly_slope_ms`, `weekly_slope_pct`, `mean_avg_ms`

---

## Algorithm

### Step 1 — Compute weekly averages
Aggregate `feature_daily_stats` by ISO week for the last N weeks:

```sql
WITH weekly AS (
    SELECT
        feature,
        feature_type,
        DATE_TRUNC('week', stat_date)                           AS week_start,
        EPOCH(DATE_TRUNC('week', stat_date)) / 604800           AS epoch_week,  -- week number for regression
        AVG(avg_ms)                                             AS weekly_avg_ms,
        AVG(p95_ms)                                             AS weekly_p95_ms,
        SUM(event_count)                                        AS weekly_events
    FROM feature_daily_stats
    WHERE stat_date >= CURRENT_DATE - INTERVAL (? || ' weeks')
      AND event_count >= 10                                     -- ignore near-zero activity weeks
    GROUP BY feature, feature_type, DATE_TRUNC('week', stat_date), epoch_week
)
```

### Step 2 — Linear regression slope per feature

```sql
SELECT
    feature,
    feature_type,
    COUNT(*)                                                    AS data_points,
    REGR_SLOPE(weekly_avg_ms, epoch_week)                       AS slope_ms_per_week,
    AVG(weekly_avg_ms)                                          AS mean_avg_ms,
    REGR_SLOPE(weekly_avg_ms, epoch_week) / NULLIF(AVG(weekly_avg_ms), 0) * 100
                                                                AS slope_pct_per_week,
    REGR_R2(weekly_avg_ms, epoch_week)                          AS r_squared,   -- trend confidence
    MIN(weekly_avg_ms)                                          AS min_avg_ms,
    MAX(weekly_avg_ms)                                          AS max_avg_ms
FROM weekly
GROUP BY feature, feature_type
HAVING COUNT(*) >= 4                                            -- at least 4 data points
   AND REGR_SLOPE(weekly_avg_ms, epoch_week) / NULLIF(AVG(weekly_avg_ms), 0) * 100 > ?  -- threshold
ORDER BY slope_pct_per_week DESC
```

### Step 3 — Filter and rank

Results are ranked by `slope_pct_per_week` descending. Only features with:
- `r_squared > 0.6` (reasonably consistent trend, not noise)
- `slope_pct_per_week > threshold` (default 3%)
- At least 4 weeks of data

are surfaced as "silent degraders".

---

## API Endpoint

```
GET /api/analytics/silent-degrader
  ?weeks=8                    # lookback window (default 8)
  &slope_threshold_pct=3.0    # minimum slope to flag (default 3%)
  &feature_type=              # optional filter
  &min_r_squared=0.6          # minimum trend confidence
```

### Response
```json
{
  "meta": { "weeks": 8, "slope_threshold_pct": 3.0, "computed_at": "2026-03-19T..." },
  "data": [
    {
      "feature": "Refresh Blotter",
      "feature_type": "BLOTTER",
      "slope_ms_per_week": 185.4,
      "slope_pct_per_week": 5.3,
      "r_squared": 0.91,
      "mean_avg_ms": 3500,
      "current_avg_ms": 4820,
      "data_points": 8,
      "weekly_series": [
        { "week": "2026-01-20", "avg_ms": 3220 },
        { "week": "2026-01-27", "avg_ms": 3380 },
        ...
      ]
    }
  ]
}
```

---

## Frontend Visualization

**Page: `SilentDegrader.tsx`**

- **Top panel**: Ranked table of flagged features — `feature`, `slope_pct_per_week`, `r_squared`, `mean_avg_ms → current_avg_ms` with color coding (yellow ≥3%, red ≥7%)
- **Bottom panel**: On row click, show a multi-line time series for that feature:
  - Line 1: weekly `avg_ms` (solid)
  - Line 2: weekly `p95_ms` (dashed)
  - Overlay: linear regression trend line
  - Annotation: "at this rate, +X% in 4 more weeks"

ECharts `LineChart` with `markLine` for the regression overlay.

---

## Inputs to Mock

- 3 features with injected +6%/week slope for 10 weeks
- 1 feature with high `r_squared` but below threshold (to test filtering)
- Remaining features flat or noisy (low `r_squared`)
