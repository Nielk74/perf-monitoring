# UC-05 — The Feature Sunset

## Problem Statement

Quantify exactly which parts of the application have zero engagement over a rolling 12-month window. Provide the evidence base needed to decommission "zombie features" — reducing testing surface area, maintenance burden, and cognitive overhead for new developers.

---

## Input Data

- `feature_daily_stats` (derived, materialized nightly):
  - `stat_date`, `feature`, `feature_type`, `event_count`, `distinct_users`

---

## Algorithm

### Step 1 — Compute rolling 12-month usage per feature

```sql
WITH feature_usage AS (
    SELECT
        feature,
        feature_type,
        SUM(event_count)                    AS total_events_12m,
        SUM(distinct_users)                 AS total_user_days_12m,   -- not distinct, sum of daily distinct
        COUNT(DISTINCT stat_date)           AS active_days_12m,       -- days with at least 1 event
        MAX(stat_date)                      AS last_used_date,
        MIN(stat_date)                      AS first_seen_date
    FROM feature_daily_stats
    WHERE stat_date >= CURRENT_DATE - INTERVAL '12 months'
    GROUP BY feature, feature_type
),
-- Also check features that exist in older data but have zero events in the window
all_features AS (
    SELECT DISTINCT feature, feature_type FROM feature_daily_stats
)
SELECT
    af.feature,
    af.feature_type,
    COALESCE(fu.total_events_12m, 0)      AS total_events_12m,
    COALESCE(fu.active_days_12m, 0)       AS active_days_12m,
    fu.last_used_date,
    fu.first_seen_date,
    CASE
        WHEN fu.total_events_12m IS NULL OR fu.total_events_12m = 0 THEN 'zombie'
        WHEN fu.active_days_12m < 10 THEN 'near-zombie'
        WHEN fu.total_events_12m < 50 THEN 'low-usage'
        ELSE 'active'
    END                                   AS status
FROM all_features af
LEFT JOIN feature_usage fu USING (feature, feature_type)
ORDER BY total_events_12m ASC
```

### Step 2 — Distinct user count over 12 months

For zombie candidates, verify with distinct user count (not just event count):

```sql
SELECT
    feature,
    COUNT(DISTINCT asmd_user)   AS distinct_users_12m
FROM audit_events
WHERE mod_dt >= CURRENT_DATE - INTERVAL '12 months'
  AND feature IN (SELECT feature FROM zombie_candidates)
GROUP BY feature
```

### Step 3 — Last-used-by breakdown

For near-zombie features, show who last used them and when:

```sql
SELECT
    feature,
    asmd_user,
    MAX(mod_dt) AS last_used
FROM audit_events
WHERE feature = :feature
GROUP BY feature, asmd_user
ORDER BY last_used DESC
LIMIT 10
```

---

## API Endpoint

```
GET /api/analytics/feature-sunset
  ?months=12                      # rolling window (default 12)
  &min_events_threshold=0         # features with <= N events are flagged (default 0)
  &feature_type=                  # optional filter
```

### Response
```json
{
  "meta": {
    "window_months": 12,
    "total_features": 48,
    "zombie_count": 3,
    "near_zombie_count": 4,
    "low_usage_count": 7
  },
  "features": [
    {
      "feature": "Legacy Audit Trail Report",
      "feature_type": "REPORT",
      "status": "zombie",
      "total_events_12m": 0,
      "active_days_12m": 0,
      "last_used_date": "2025-09-14",
      "first_seen_date": "2025-03-01",
      "distinct_users_12m": 0
    },
    {
      "feature": "Old Batch Processor",
      "feature_type": "TOOLS",
      "status": "zombie",
      "total_events_12m": 0,
      "active_days_12m": 0,
      "last_used_date": "2025-08-22",
      "first_seen_date": "2025-03-01",
      "distinct_users_12m": 0
    },
    {
      "feature": "Search Portfolio",
      "feature_type": "SEARCH",
      "status": "active",
      "total_events_12m": 182440,
      "active_days_12m": 312,
      "last_used_date": "2026-03-19",
      "distinct_users_12m": 67
    }
  ]
}
```

---

## Frontend Visualization

**Page: `FeatureSunset.tsx`**

**Panels**:
1. **Summary badges**: 3 cards — "X Zombie Features", "Y Near-Zombie", "Z Low-Usage" with traffic-light colors
2. **Feature status table**: all features with columns: `feature`, `feature_type`, `status` (badge), `last_used_date`, `total_events_12m`, `distinct_users_12m`. Sortable. Row color: red=zombie, orange=near-zombie, yellow=low-usage, white=active.
3. **Timeline bar chart** (horizontal): features on Y-axis, bars show the date range of activity within the 12-month window (start bar at `first_seen_date`, end bar at `last_used_date`). Gaps are visible. Zombie features show only a marker at last use.
4. **Drill-down**: click a feature to see "last used by" table — user + date

---

## Inputs to Mock

- `Legacy Audit Trail Report`: last event 6 months ago, then silence
- `Old Batch Processor`: last event 7 months ago, then silence
- `Archive Data Viewer`: last event 5.5 months ago, then silence
- `Show Legacy FX Rates`: used by 2 users only, last event 3 months ago (near-zombie)
