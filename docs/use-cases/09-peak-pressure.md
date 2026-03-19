# UC-09 — The Peak Pressure Forecast

## Problem Statement

Analyze usage density across time zones, hours, and calendar dates (particularly month-end closing). Predict exactly when the system will be under maximum stress so that infrastructure can be pre-scaled, maintenance windows can be scheduled safely, and on-call teams can be adequately staffed.

---

## Input Data

- `feature_daily_stats` (for daily totals)
- `audit_events` (for hour-of-day granularity — not pre-aggregated)

---

## Algorithm

### View 1: Hour × Day-of-Week Heatmap

Aggregate events by local time (converted from UTC using workstation office):

```sql
SELECT
    EXTRACT(DOW FROM mod_dt AT TIME ZONE 'UTC')     AS day_of_week,  -- 0=Sun, 6=Sat
    EXTRACT(HOUR FROM mod_dt AT TIME ZONE 'UTC')    AS hour_utc,
    COUNT(*)                                         AS event_count,
    COUNT(DISTINCT asmd_user)                        AS active_users,
    AVG(duration_ms) FILTER (WHERE duration_ms IS NOT NULL) AS avg_ms
FROM audit_events
WHERE mod_dt BETWEEN :start_date AND :end_date
  AND (:office IS NULL OR workstation LIKE :office || '%')
GROUP BY day_of_week, hour_utc
ORDER BY day_of_week, hour_utc
```

This produces a 7×24 grid used as the heatmap source.

### View 2: Calendar Daily Heatmap

```sql
SELECT
    CAST(mod_dt AS DATE)    AS event_date,
    COUNT(*)                AS event_count,
    COUNT(DISTINCT asmd_user) AS active_users,
    EXTRACT(DAY FROM (DATE_TRUNC('month', CAST(mod_dt AS DATE) + INTERVAL '1 month') - CAST(mod_dt AS DATE)))
                            AS days_to_month_end
FROM audit_events
WHERE mod_dt BETWEEN :start_date AND :end_date
GROUP BY CAST(mod_dt AS DATE)
ORDER BY event_date
```

### View 3: Month-End Detection

```sql
-- Flag days within 2 business days of month end
SELECT
    event_date,
    event_count,
    active_users,
    days_to_month_end,
    days_to_month_end <= 2                          AS is_month_end,
    event_count * 1.0 / AVG(event_count) OVER ()   AS load_factor   -- relative to mean
FROM (
    SELECT
        CAST(mod_dt AS DATE)    AS event_date,
        COUNT(*)                AS event_count,
        COUNT(DISTINCT asmd_user) AS active_users,
        EXTRACT(DAY FROM (DATE_TRUNC('month', CAST(mod_dt AS DATE) + INTERVAL '1 month') - CAST(mod_dt AS DATE))) AS days_to_month_end
    FROM audit_events
    WHERE mod_dt BETWEEN :start_date AND :end_date
    GROUP BY CAST(mod_dt AS DATE)
)
ORDER BY event_date
```

### View 4: Peak Hours by Office

Breakdown per office prefix — each office has different working hours:

```sql
SELECT
    SUBSTR(workstation, 1, 3)                   AS office,
    EXTRACT(HOUR FROM mod_dt AT TIME ZONE 'UTC') AS hour_utc,
    COUNT(*)                                     AS event_count
FROM audit_events
WHERE mod_dt BETWEEN :start_date AND :end_date
GROUP BY SUBSTR(workstation, 1, 3), EXTRACT(HOUR FROM mod_dt AT TIME ZONE 'UTC')
ORDER BY office, hour_utc
```

### Forecast (simple extrapolation)

For upcoming month-end dates, project expected load based on historical month-end multiplier:

```python
# In analytics module (Python post-SQL)
month_end_factor = avg_month_end_events / avg_normal_events
# Apply to next 3 upcoming month-end dates
upcoming_peaks = [
    {
        "date": next_month_end,
        "expected_events": avg_daily_events * month_end_factor,
        "confidence": "based on 12-month average"
    }
    for next_month_end in compute_upcoming_month_ends(n=3)
]
```

---

## API Endpoint

```
GET /api/analytics/peak-pressure
  ?start_date=2026-01-01
  &end_date=2026-03-19
  &granularity=hour          # hour | day | week
  &office=                   # optional office prefix filter
```

### Response
```json
{
  "meta": {
    "start_date": "2026-01-01",
    "end_date": "2026-03-19",
    "avg_daily_events": 14820,
    "peak_daily_events": 46200,
    "peak_date": "2026-01-31",
    "month_end_load_factor": 3.1
  },
  "hourly_heatmap": [
    { "day_of_week": 1, "hour_utc": 8,  "event_count": 1240, "active_users": 38 },
    { "day_of_week": 1, "hour_utc": 9,  "event_count": 2180, "active_users": 62 }
  ],
  "daily_series": [
    { "date": "2026-01-31", "event_count": 46200, "active_users": 82, "is_month_end": true, "load_factor": 3.12 },
    { "date": "2026-02-01", "event_count": 14100, "active_users": 71, "is_month_end": false, "load_factor": 0.95 }
  ],
  "by_office": [
    { "office": "LON", "peak_hour_utc": 9, "peak_event_count": 840 },
    { "office": "NYC", "peak_hour_utc": 14, "peak_event_count": 620 },
    { "office": "SIN", "peak_hour_utc": 2, "peak_event_count": 310 }
  ],
  "upcoming_peaks": [
    { "date": "2026-03-31", "expected_events": 45900, "is_month_end": true },
    { "date": "2026-04-30", "expected_events": 46400, "is_month_end": true }
  ]
}
```

---

## Frontend Visualization

**Page: `PeakPressure.tsx`**

**Panels**:
1. **Hour × Day-of-Week heatmap**: 7 columns (Mon–Sun) × 24 rows (00:00–23:00 UTC). Cell color intensity = `event_count`. Tooltips show `active_users` and `avg_ms`. Immediately reveals the "danger hours" for each weekday.

2. **Calendar heatmap** (GitHub-style): 12 months of daily squares colored by `load_factor`. Month-end days highlighted with a red border. Shows seasonal patterns and month-end clustering at a glance.

3. **Office peak hours** (overlaid line chart): one line per office showing event count by hour of the UTC day. Shows the "handoff" between time zones as the activity wave moves from SIN → LON → NYC.

4. **Upcoming peaks table**: next 3 month-end dates with projected load factor and a recommended prep note ("Scale infrastructure 24h before").

ECharts: `HeatmapChart` (for both heatmap types), `LineChart` (office overlap).

---

## Inputs to Mock

- Month-end days: 3× normal event volume (last 2 business days of each month)
- LON peak: 08:00–10:00 UTC (market open)
- NYC peak: 13:00–16:00 UTC (US market hours)
- SIN peak: 01:00–03:00 UTC (Asian market hours)
- Monday is consistently heavier than Friday across all offices
- 3 specific dates injected with 4× normal volume (simulates quarter-end)
