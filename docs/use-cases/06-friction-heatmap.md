# UC-06 — The Friction Heatmap

## Problem Statement

Rank features not by usage frequency alone, but by the cumulative "wait time" they impose on the entire company. A feature used 10,000 times with a 5-second load contributes 50,000 seconds (13.9 hours) of lost productivity. This metric directly prioritizes technical debt by business ROI: fixing the top 5 features by total wait time returns the most hours to the organization.

---

## Input Data

- `feature_daily_stats` (derived, materialized nightly):
  - `feature`, `feature_type`, `event_count`, `total_wait_ms`, `avg_ms`, `p95_ms`, `distinct_users`

---

## Algorithm

### Step 1 — Aggregate total wait time per feature

```sql
SELECT
    feature,
    feature_type,
    SUM(event_count)                        AS total_events,
    SUM(total_wait_ms)                      AS total_wait_ms,
    SUM(total_wait_ms) / 3600000.0          AS total_wait_hours,
    AVG(avg_ms)                             AS avg_ms,
    PERCENTILE_CONT(0.95) WITHIN GROUP (ORDER BY p95_ms) AS p95_ms,
    MAX(distinct_users)                     AS peak_daily_users
FROM feature_daily_stats
WHERE stat_date BETWEEN :start_date AND :end_date
  AND total_wait_ms IS NOT NULL
GROUP BY feature, feature_type
ORDER BY total_wait_ms DESC
```

### Step 2 — Compute productivity loss estimate

Convert total wait time into business hours:

```python
# In the analytics module, post-SQL computation
for row in results:
    # Assume 240 working days/year, 8h/day = 1920 working hours
    row["user_days_lost"] = row["total_wait_hours"] / 8.0
    row["annual_projection"] = row["total_wait_hours"] * (365 / date_range_days)
```

### Step 3 — Heatmap view (feature × week)

For the heatmap visualization, compute weekly total wait time per feature:

```sql
SELECT
    feature,
    feature_type,
    DATE_TRUNC('week', stat_date)           AS week_start,
    SUM(total_wait_ms) / 3600000.0          AS wait_hours_that_week
FROM feature_daily_stats
WHERE stat_date BETWEEN :start_date AND :end_date
  AND total_wait_ms IS NOT NULL
GROUP BY feature, feature_type, DATE_TRUNC('week', stat_date)
ORDER BY feature, week_start
```

---

## API Endpoint

```
GET /api/analytics/friction-heatmap
  ?start_date=2026-01-01
  &end_date=2026-03-19
  &feature_type=             # optional filter
  &top_n=30                  # return top N features by total_wait_ms (default 30)
```

### Response
```json
{
  "meta": {
    "start_date": "2026-01-01",
    "end_date": "2026-03-19",
    "date_range_days": 78,
    "total_wait_hours_all_features": 1284.5
  },
  "ranked": [
    {
      "rank": 1,
      "feature": "Refresh Blotter",
      "feature_type": "BLOTTER",
      "total_events": 94210,
      "total_wait_ms": 329735000,
      "total_wait_hours": 91.6,
      "avg_ms": 3500,
      "p95_ms": 8200,
      "user_days_lost": 11.4,
      "annual_projection_hours": 429
    },
    {
      "rank": 2,
      "feature": "Run P&L Report",
      "feature_type": "REPORT",
      "total_events": 18400,
      "total_wait_ms": 276000000,
      "total_wait_hours": 76.7,
      "avg_ms": 15000,
      "p95_ms": 32000,
      "user_days_lost": 9.6,
      "annual_projection_hours": 359
    }
  ],
  "weekly_heatmap": [
    { "feature": "Refresh Blotter", "week_start": "2026-01-05", "wait_hours": 7.2 },
    { "feature": "Refresh Blotter", "week_start": "2026-01-12", "wait_hours": 8.1 }
  ]
}
```

---

## Frontend Visualization

**Page: `FrictionHeatmap.tsx`**

**Panels**:
1. **Ranked bar chart** (horizontal): features on Y-axis, `total_wait_hours` on X-axis. Bars colored by `feature_type`. A secondary axis shows `total_events`. This is the primary "prioritization" view — which features to fix first.

2. **Annotation overlay**: on each bar, show `avg_ms` as text label ("avg 3.5s per call").

3. **Heatmap matrix** (below): X-axis = weeks, Y-axis = top 20 features (same order as ranked list). Color intensity = `wait_hours_that_week`. This shows whether the friction is growing, shrinking, or stable over time.

4. **ROI summary table**: for the top 10 features, show a business case row:
   - "If `Refresh Blotter` is optimized to 1s (from 3.5s avg), the company saves **78 hours/year**"
   - Computed as: `(current_avg_ms - target_ms) / 1000 * total_events / 3600`

ECharts `BarChart` (horizontal) + `HeatmapChart` (matrix).

---

## Inputs to Mock

- `Refresh Blotter`: 94,000 events × 3,500ms avg → clear #1 friction source
- `Run P&L Report`: 18,400 events × 15,000ms avg → #2 despite lower frequency
- `Copy Contract No`: 250,000 events but 0ms (no duration) → excluded from heatmap but shown in ranked table as N/A
- Weekly pattern: friction spikes at month-end (last 2 days of month have 3× events)
