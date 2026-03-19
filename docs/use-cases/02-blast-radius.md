# UC-02 — The Blast Radius

## Problem Statement

When a specific commit is identified as problematic, instantly visualize every user and workstation impacted during the deployment window. This enables surgical rollbacks and targeted communication to affected desks — without guessing who is affected.

---

## Input Data

- `audit_events` — user actions with timestamps
- `commits` — with `deployed_at` (time this commit went live)
- Requires at least some commits to have `deployed_at` populated

---

## Algorithm

### Step 1 — Resolve the deployment window

Given a `commit_hash` (or `tag`), find the deployment window:
- **Start**: `commits.deployed_at` for the given commit
- **End**: `deployed_at` of the **next** commit (or `NOW()` if it's the latest)

```sql
WITH ordered_commits AS (
    SELECT
        commit_hash,
        tag,
        deployed_at,
        LEAD(deployed_at) OVER (ORDER BY deployed_at) AS next_deployed_at
    FROM commits
    WHERE deployed_at IS NOT NULL
)
SELECT
    deployed_at     AS window_start,
    COALESCE(next_deployed_at, NOW()) AS window_end
FROM ordered_commits
WHERE commit_hash = ?
```

### Step 2 — Find all events in the window

```sql
SELECT
    asmd_user,
    workstation,
    SUBSTR(workstation, 1, 3)   AS office,
    feature_type,
    feature,
    COUNT(*)                    AS event_count,
    AVG(duration_ms)            AS avg_ms_in_window
FROM audit_events
WHERE mod_dt BETWEEN :window_start AND :window_end
GROUP BY asmd_user, workstation, office, feature_type, feature
ORDER BY event_count DESC
```

### Step 3 — Compute the performance delta

To assess if the commit actually caused a regression, compare the in-window performance to the 4-week baseline prior to deployment:

```sql
-- Baseline: same features, 4 weeks before window_start
SELECT feature, AVG(avg_ms) AS baseline_avg_ms
FROM feature_daily_stats
WHERE stat_date BETWEEN :window_start - INTERVAL '4 weeks' AND :window_start
GROUP BY feature
```

Join with window stats to compute `delta_pct = (avg_ms_in_window - baseline_avg_ms) / baseline_avg_ms * 100`.

---

## API Endpoint

```
GET /api/analytics/blast-radius?commit_hash=abc123def456
GET /api/analytics/blast-radius?tag=v2.14.0
```

### Response
```json
{
  "meta": {
    "commit_hash": "abc123def456",
    "tag": "v2.14.0",
    "author": "Jane Smith",
    "committed_at": "2026-02-10T14:22:00Z",
    "deployed_at": "2026-02-10T16:45:00Z",
    "window_start": "2026-02-10T16:45:00Z",
    "window_end": "2026-02-11T09:30:00Z",
    "total_users_impacted": 47,
    "total_workstations": 62,
    "total_events": 8420
  },
  "by_office": [
    { "office": "LON", "users": 28, "workstations": 38, "events": 5100 },
    { "office": "NYC", "users": 14, "workstations": 18, "events": 2800 }
  ],
  "by_feature": [
    {
      "feature": "Refresh Blotter",
      "feature_type": "BLOTTER",
      "event_count": 1240,
      "avg_ms_in_window": 6820,
      "baseline_avg_ms": 3500,
      "delta_pct": 94.9
    }
  ],
  "impacted_users": [
    { "asmd_user": "MS01", "workstation": "LONW00087286", "office": "LON", "event_count": 145 }
  ],
  "changed_files": [
    { "file_path": "src/features/blotter/refresh.py", "change_type": "M", "lines_added": 42, "lines_removed": 8 }
  ]
}
```

---

## Frontend Visualization

**Page: `BlastRadius.tsx`**

**Controls**:
- Commit selector dropdown (shows recent commits with tag/hash/author/date)
- Or: paste a commit hash directly

**Panels**:
1. **Summary cards**: total impacted users, workstations, events, time window duration
2. **Office treemap**: each rectangle = office, sized by `event_count`, colored by `delta_pct` (green = no regression, red = severe regression)
3. **Feature regression table**: features sorted by `delta_pct` descending — shows baseline vs window performance
4. **Impacted users list**: scrollable table of users + workstations, with office grouping
5. **Changed files list**: from `commit_files` — helps correlate which code change caused which feature regression

---

## Inputs to Mock

- 2 commits tagged `v2.12.0` and `v2.14.0` with `deployed_at` set
- In the window after `v2.14.0`: `Refresh Blotter` avg_ms is 2× baseline for all LON users
- `v2.12.0`: `Run P&L Report` is 1.5× baseline, affecting 12 NYC users only
