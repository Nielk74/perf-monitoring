# UC-07 — The Workflow Discovery

## Problem Statement

Map the common sequences of actions users take through the application. If users consistently jump from "Search" → "Tools" → "Static Data," this signals a need for a unified interface that streamlines that specific business process. The Gantt timeline view gives a granular, per-session picture of how users actually move through the application.

This use case has two complementary views:
1. **Transition matrix / Sankey**: aggregate flow patterns across all users
2. **Gantt timeline**: per-user session view, showing feature actions as time bars

---

## Input Data

- `feature_sequences` (materialized nightly): pre-computed transition frequencies
- `audit_events`: for Gantt (per-user, per-day query — interactive, not pre-aggregated)
- `user_sessions` (materialized): session boundaries

---

## Algorithm: Transition Matrix

### Pre-computation (nightly, stored in `feature_sequences`)

Session reconstruction uses a 30-minute inactivity gap to define session boundaries:

```sql
WITH ordered_events AS (
    SELECT
        asmd_user,
        workstation,
        mod_dt,
        feature,
        feature_type,
        LAG(feature) OVER (PARTITION BY asmd_user, workstation ORDER BY mod_dt) AS prev_feature,
        LAG(feature_type) OVER (PARTITION BY asmd_user, workstation ORDER BY mod_dt) AS prev_feature_type,
        LAG(mod_dt) OVER (PARTITION BY asmd_user, workstation ORDER BY mod_dt) AS prev_dt,
        EPOCH(mod_dt) - EPOCH(LAG(mod_dt) OVER (PARTITION BY asmd_user, workstation ORDER BY mod_dt)) AS gap_seconds
    FROM audit_events
    WHERE supp_user = asmd_user   -- exclude support sessions
),
transitions AS (
    SELECT
        prev_feature        AS from_feature,
        feature             AS to_feature,
        prev_feature_type   AS from_feature_type,
        feature_type        AS to_feature_type,
        gap_seconds
    FROM ordered_events
    WHERE prev_feature IS NOT NULL
      AND gap_seconds < 1800   -- within same session (< 30 min)
      AND from_feature != to_feature   -- ignore self-transitions
)
INSERT OR REPLACE INTO feature_sequences
SELECT
    from_feature,
    to_feature,
    from_feature_type,
    to_feature_type,
    COUNT(*)            AS transition_count,
    AVG(gap_seconds)    AS avg_gap_seconds
FROM transitions
GROUP BY from_feature, to_feature, from_feature_type, to_feature_type
```

---

## API Endpoint: Transitions (Sankey)

```
GET /api/analytics/workflow-discovery/transitions
  ?min_count=50          # minimum transition count to include (default 50)
  &feature_type=         # optional: filter source or target feature type
  &top_n=40              # max edges to return (default 40)
```

### Response
```json
{
  "meta": { "total_distinct_transitions": 187, "min_count": 50, "top_n": 40 },
  "nodes": [
    { "id": "Search Contract", "feature_type": "SEARCH", "total_outgoing": 48200 },
    { "id": "Open Blotter", "feature_type": "BLOTTER", "total_outgoing": 31400 }
  ],
  "links": [
    {
      "from": "Search Contract",
      "to": "Open Blotter",
      "count": 28140,
      "avg_gap_seconds": 8.4,
      "pct_of_from": 58.4
    },
    {
      "from": "Open Blotter",
      "to": "Export to Excel",
      "count": 18900,
      "avg_gap_seconds": 42.1,
      "pct_of_from": 60.2
    }
  ]
}
```

`pct_of_from`: what percentage of `from_feature` usages lead to `to_feature`. This surfaces the dominant paths.

---

## API Endpoint: Gantt (Per User / Per Day)

```
GET /api/analytics/workflow-discovery/gantt
  ?asmd_user=MS01
  &date=2026-03-12
```

### Response
```json
{
  "meta": {
    "asmd_user": "MS01",
    "date": "2026-03-12",
    "total_events": 87,
    "sessions": 2
  },
  "sessions": [
    {
      "session_index": 1,
      "session_start": "2026-03-12T08:44:00Z",
      "session_end": "2026-03-12T12:10:00Z",
      "events": [
        {
          "feature": "Star Startup",
          "feature_type": "SYSTEM",
          "start": "2026-03-12T08:44:00Z",
          "end": "2026-03-12T08:46:23Z",
          "duration_ms": 143000
        },
        {
          "feature": "Search Contract",
          "feature_type": "SEARCH",
          "start": "2026-03-12T08:47:10Z",
          "end": "2026-03-12T08:47:11Z",
          "duration_ms": 820
        }
      ]
    }
  ]
}
```

> **Note**: for events without `duration_ms`, `end = start + 1 second` (instant action). The Gantt still renders a minimal bar for visibility.

---

## Frontend Visualization

**Page: `WorkflowDiscovery.tsx`**

Two tabs:

### Tab 1: Flow Patterns (Sankey)
- **ECharts Sankey diagram**: nodes = features (colored by `feature_type`), edges = transitions (thickness = `count`)
- Controls: `min_count` slider, `feature_type` filter
- Click a node to highlight all paths through it
- Tooltip shows: `count`, `avg_gap_seconds`, `pct_of_from`

### Tab 2: Session Gantt
- **Controls**: user selector + date picker
- **ECharts Custom series Gantt**:
  - Y-axis: session index (Session 1, Session 2, ...)
  - X-axis: time (hour:minute)
  - Each bar = one feature action, colored by `feature_type`
  - Bar width proportional to `duration_ms` (instant actions shown as thin markers)
  - Gaps between bars = time between actions (idle time visible)
- Tooltip on hover: feature name, exact time, duration in ms
- Session boundaries marked with a vertical dashed separator

---

## Inputs to Mock

### Transition patterns
- `Search Contract → Open Blotter`: 58% of Search Contract events lead to Open Blotter
- `Open Blotter → Export to Excel`: 60% of Open Blotter events lead to Export to Excel
- `Open Static Data → Edit Counterparty`: 80% co-occurrence within sessions
- Weaker pattern: `Run P&L Report → Run Position Report`: 35%

### Gantt data
- User `MS01` on 2026-03-12 has 2 sessions:
  - Morning: Startup → Search → Blotter → Export
  - Afternoon: P&L Report → Position Report → Static Data → Edit Counterparty
- Gaps and durations set to make the Gantt visually interesting
