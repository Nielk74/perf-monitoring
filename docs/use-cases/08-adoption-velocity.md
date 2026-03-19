# UC-08 — The Adoption Velocity

## Problem Statement

Measure how quickly a new feature is picked up by the user base after a release. This helps the Product Owner evaluate the success of new launches and the effectiveness of user training. An S-curve of adoption over 90 days tells you whether the feature is spreading organically or stalling.

---

## Input Data

- `audit_events` — first appearance of a feature per user defines "adoption"
- `commits` — `deployed_at` or `tag` can define the launch date
- The feature's "launch date" can be:
  - Auto-detected as `MIN(mod_dt)` for that feature in `audit_events`
  - Manually specified via API parameter

---

## Algorithm

### Step 1 — Find first use per user (adoption event)

```sql
SELECT
    asmd_user,
    MIN(mod_dt)     AS first_used_at
FROM audit_events
WHERE feature = :feature
  AND mod_dt >= :launch_date
GROUP BY asmd_user
```

### Step 2 — Build cumulative adoption curve

```sql
WITH adoptions AS (
    SELECT
        asmd_user,
        MIN(mod_dt) AS first_used_at
    FROM audit_events
    WHERE feature = :feature
      AND mod_dt >= :launch_date
    GROUP BY asmd_user
),
daily_new AS (
    SELECT
        CAST(first_used_at AS DATE)     AS adoption_date,
        COUNT(*)                        AS new_users
    FROM adoptions
    GROUP BY CAST(first_used_at AS DATE)
),
date_series AS (
    SELECT UNNEST(GENERATE_SERIES(
        CAST(:launch_date AS DATE),
        CAST(:launch_date AS DATE) + INTERVAL (:days || ' days'),
        INTERVAL '1 day'
    )) AS d
),
cumulative AS (
    SELECT
        ds.d                                                    AS date,
        COALESCE(dn.new_users, 0)                               AS new_users_that_day,
        SUM(COALESCE(dn.new_users, 0)) OVER (ORDER BY ds.d)    AS cumulative_users
    FROM date_series ds
    LEFT JOIN daily_new dn ON ds.d = dn.adoption_date
)
SELECT
    date,
    new_users_that_day,
    cumulative_users,
    -- Total eligible user pool (distinct users active in the 30 days before launch)
    :eligible_users AS eligible_users,
    cumulative_users * 100.0 / NULLIF(:eligible_users, 0) AS adoption_pct
FROM cumulative
ORDER BY date
```

### Step 3 — Compute eligible user pool

The denominator for adoption percentage: distinct users who used the application in the 30 days before the launch date (they had access but hadn't adopted yet):

```sql
SELECT COUNT(DISTINCT asmd_user) AS eligible_users
FROM audit_events
WHERE mod_dt BETWEEN :launch_date - INTERVAL '30 days' AND :launch_date
```

### Step 4 — Usage intensity (post-adoption)

After adopting, how often do users use the feature?

```sql
SELECT
    asmd_user,
    COUNT(*)                        AS uses_since_adoption,
    AVG(duration_ms)                AS avg_ms,
    MAX(mod_dt)                     AS last_used_at,
    MAX(mod_dt) >= CURRENT_DATE - INTERVAL '7 days' AS is_active_this_week
FROM audit_events
WHERE feature = :feature
  AND mod_dt >= :launch_date
GROUP BY asmd_user
```

---

## API Endpoint

```
GET /api/analytics/adoption-velocity
  ?feature=New+Blotter+View
  &launch_date=2026-01-15      # ISO date; auto-detected if omitted
  &days=90                     # lookback from launch (default 90)
```

### Response
```json
{
  "meta": {
    "feature": "New Blotter View",
    "feature_type": "BLOTTER",
    "launch_date": "2026-01-15",
    "days": 90,
    "eligible_users": 85,
    "total_adopters": 61,
    "adoption_pct": 71.8,
    "still_active": 54,
    "median_days_to_adopt": 12
  },
  "curve": [
    { "date": "2026-01-15", "new_users": 3, "cumulative_users": 3, "adoption_pct": 3.5 },
    { "date": "2026-01-16", "new_users": 5, "cumulative_users": 8, "adoption_pct": 9.4 },
    ...
    { "date": "2026-04-15", "new_users": 0, "cumulative_users": 61, "adoption_pct": 71.8 }
  ],
  "by_office": [
    { "office": "LON", "adopters": 38, "eligible": 50, "adoption_pct": 76.0 },
    { "office": "NYC", "adopters": 15, "eligible": 25, "adoption_pct": 60.0 }
  ],
  "non_adopters": ["JD04", "PK07", "..."]
}
```

---

## Frontend Visualization

**Page: `AdoptionVelocity.tsx`**

**Controls**:
- Feature selector (dropdown)
- Launch date picker (with "auto-detect" checkbox)
- Days slider (30 / 60 / 90 / 180)

**Panels**:
1. **Summary cards**: total adopters, adoption %, still active this week, median days to adopt
2. **S-curve area chart** (primary): X-axis = days since launch, Y-axis = cumulative adopters (or `adoption_pct`). Area fill. Reference lines at 25%, 50%, 75% thresholds. A steeper curve = faster adoption.
3. **Daily new adopters bar chart** (secondary, below): day-by-day new user count overlaid as a bar histogram under the area chart. Peaks correspond to training sessions, announcements, or rollout phases.
4. **Office adoption breakdown**: horizontal stacked bar — `adopters` vs `non-adopters` per office
5. **Non-adopters list**: users who are eligible but haven't used the feature yet — actionable for targeted training outreach

ECharts `LineChart` with area fill + `BarChart` overlay (dual Y-axis).

---

## Inputs to Mock

- Feature `New Blotter View` first appears 90 days before "today"
- Days 1–14: slow pickup (3–5 new users/day) — early adopters + power users
- Days 15–45: acceleration (8–12 new users/day) — bulk rollout / training
- Days 46–90: plateau (0–2 new users/day)
- Final adoption: 72% of eligible users (25% non-adopters remain)
- LON office adopts faster than NYC (LON gets training first in the mock)
