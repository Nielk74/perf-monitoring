# UC-04 — The Impersonation Audit

## Problem Statement

Validate whether internal support interventions (where a support user logs in as another user) actually resolve the user's performance friction. By comparing support-led sessions against standard user sessions in the same time window, we can objectively measure if support resolved the issue or merely masked it.

The audit also provides a full trail of which support users acted as which end users — important for compliance and access governance.

---

## Input Data

Impersonation is identified when `supp_user != asmd_user` in `audit_events`.

- `audit_events` — all events, normal and impersonated
- `user_sessions` — materialized, includes `is_impersonated` flag

---

## Algorithm

### Step 1 — Identify impersonated sessions

```sql
SELECT
    supp_user,
    asmd_user,
    workstation,
    mod_dt,
    feature_type,
    feature,
    duration_ms
FROM audit_events
WHERE supp_user != asmd_user
  AND mod_dt BETWEEN :start_date AND :end_date
  AND (:asmd_user IS NULL OR asmd_user = :asmd_user)
```

### Step 2 — Compare support vs normal session performance

For each `asmd_user` + `feature` combination that appears in both impersonated and non-impersonated sessions within the same time window:

```sql
WITH normal AS (
    SELECT
        asmd_user,
        feature,
        AVG(duration_ms) AS normal_avg_ms,
        COUNT(*)          AS normal_count
    FROM audit_events
    WHERE supp_user = asmd_user
      AND duration_ms IS NOT NULL
      AND mod_dt BETWEEN :start_date AND :end_date
    GROUP BY asmd_user, feature
),
support AS (
    SELECT
        asmd_user,
        feature,
        AVG(duration_ms)  AS support_avg_ms,
        COUNT(*)           AS support_count,
        supp_user
    FROM audit_events
    WHERE supp_user != asmd_user
      AND duration_ms IS NOT NULL
      AND mod_dt BETWEEN :start_date AND :end_date
    GROUP BY asmd_user, feature, supp_user
)
SELECT
    s.asmd_user,
    s.feature,
    s.supp_user,
    s.support_avg_ms,
    n.normal_avg_ms,
    (s.support_avg_ms - n.normal_avg_ms) / NULLIF(n.normal_avg_ms, 0) * 100 AS delta_pct,
    s.support_count,
    n.normal_count
FROM support s
JOIN normal n USING (asmd_user, feature)
ORDER BY delta_pct DESC
```

A positive `delta_pct` means support sessions were **slower** (friction unresolved or support overhead). A negative `delta_pct` means support sessions were **faster** (intervention helped).

### Step 3 — Support activity summary

```sql
SELECT
    supp_user,
    COUNT(DISTINCT asmd_user)       AS users_impersonated,
    COUNT(DISTINCT DATE(mod_dt))    AS active_days,
    COUNT(*)                        AS total_actions,
    MIN(mod_dt)                     AS first_session,
    MAX(mod_dt)                     AS last_session
FROM audit_events
WHERE supp_user != asmd_user
  AND mod_dt BETWEEN :start_date AND :end_date
GROUP BY supp_user
ORDER BY total_actions DESC
```

---

## API Endpoint

```
GET /api/analytics/impersonation-audit
  ?start_date=2026-03-01
  &end_date=2026-03-19
  &asmd_user=MS01          # optional: filter to one user
  &supp_user=              # optional: filter to one support user
```

### Response
```json
{
  "meta": {
    "start_date": "2026-03-01",
    "end_date": "2026-03-19",
    "total_impersonated_sessions": 47,
    "distinct_support_users": 3,
    "distinct_impersonated_users": 12
  },
  "support_summary": [
    {
      "supp_user": "SUPP01",
      "users_impersonated": 5,
      "active_days": 8,
      "total_actions": 312,
      "first_session": "2026-03-02T09:15:00Z",
      "last_session": "2026-03-18T14:40:00Z"
    }
  ],
  "feature_comparison": [
    {
      "asmd_user": "MS01",
      "feature": "Refresh Blotter",
      "supp_user": "SUPP01",
      "normal_avg_ms": 3820,
      "support_avg_ms": 3240,
      "delta_pct": -15.2,
      "support_count": 12,
      "normal_count": 145
    }
  ],
  "session_timeline": [
    {
      "supp_user": "SUPP01",
      "asmd_user": "MS01",
      "session_start": "2026-03-15T10:20:00Z",
      "session_end": "2026-03-15T10:48:00Z",
      "action_count": 24,
      "features_touched": ["Refresh Blotter", "Open Static Data"]
    }
  ]
}
```

---

## Frontend Visualization

**Page: `ImpersonationAudit.tsx`**

**Controls**:
- Date range
- `asmd_user` selector (the user being impersonated)
- `supp_user` selector (the support account)

**Panels**:
1. **Support activity table**: one row per support user — users impersonated, active days, total actions
2. **Feature comparison chart**: grouped bar chart — for each feature, two bars side-by-side: `normal_avg_ms` (blue) vs `support_avg_ms` (orange). Color the support bar red if `delta_pct > 0` (worse), green if `delta_pct < 0` (better).
3. **Session timeline**: expandable list of impersonation sessions — start/end time, duration, features touched, and the support user who performed the session

---

## Compliance Notes

This data is sensitive. In production:
- Consider rate-limiting this endpoint to admin roles only (via API key or OAuth scope)
- Logs all accesses to this endpoint in a separate audit log
- `supp_user` should never appear in public-facing dashboards

For the mock dataset, all support users are clearly prefixed (`SUPP01`, `SUPP02`, etc.) to make filtering obvious.
