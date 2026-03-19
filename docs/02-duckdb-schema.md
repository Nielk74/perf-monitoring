# 02 — DuckDB Schema

## Overview

Three layers of tables:
1. **Raw** — direct imports from Oracle and Git, never modified after insert
2. **Derived** — materialized nightly from raw data, expensive aggregations
3. **Views** — lightweight SQL shortcuts for common joins

---

## Raw Tables

### `audit_events`
Direct mapping from Oracle `STAR_ACTION_AUDIT`. One row per user action.

```sql
CREATE TABLE IF NOT EXISTS audit_events (
    id              BIGINT PRIMARY KEY,      -- synthetic surrogate, assigned on import
    supp_user       VARCHAR(20) NOT NULL,    -- actual logged-in user (support staff or normal user)
    asmd_user       VARCHAR(20) NOT NULL,    -- user being acted upon (same as supp_user if normal)
    workstation     VARCHAR(64) NOT NULL,    -- e.g. LONW00081256
    mod_dt          TIMESTAMPTZ NOT NULL,    -- action timestamp with timezone
    feature_type    VARCHAR(16) NOT NULL,    -- REPORT, SEARCH, BLOTTER, TOOLS, STATIC, SYSTEM, ...
    feature         VARCHAR(100) NOT NULL,   -- full feature name
    detail          VARCHAR(100),            -- optional extra context
    duration_ms     INTEGER,                 -- NULL for non-timed actions
    imported_at     TIMESTAMP DEFAULT now()  -- when this row was inserted into DuckDB
);

CREATE INDEX idx_audit_mod_dt       ON audit_events(mod_dt);
CREATE INDEX idx_audit_asmd_user    ON audit_events(asmd_user);
CREATE INDEX idx_audit_feature_type ON audit_events(feature_type);
CREATE INDEX idx_audit_workstation  ON audit_events(workstation);
```

**Import key**: `(asmd_user, workstation, mod_dt, feature, duration_ms)` — used to deduplicate on re-import.

**Impersonation detection**: `supp_user != asmd_user` → support session.

---

### `commits`
One row per Git commit across all tracked repositories.

```sql
CREATE TABLE IF NOT EXISTS commits (
    commit_hash     VARCHAR(40) PRIMARY KEY,
    repo_name       VARCHAR(100) NOT NULL,
    author_name     VARCHAR(200),
    author_email    VARCHAR(200),
    committed_at    TIMESTAMP NOT NULL,      -- UTC commit timestamp
    message         TEXT,
    is_merge        BOOLEAN DEFAULT false,
    tag             VARCHAR(200),            -- nearest tag (release marker), nullable
    deployed_at     TIMESTAMP,               -- estimated deploy time, nullable, set manually or via CI
    imported_at     TIMESTAMP DEFAULT now()
);

CREATE INDEX idx_commits_committed_at ON commits(committed_at);
CREATE INDEX idx_commits_deployed_at  ON commits(deployed_at);
```

---

### `commit_files`
Files changed per commit. Used to correlate commits with feature areas.

```sql
CREATE TABLE IF NOT EXISTS commit_files (
    commit_hash     VARCHAR(40) NOT NULL REFERENCES commits(commit_hash),
    file_path       VARCHAR(500) NOT NULL,
    change_type     VARCHAR(1),              -- A=added, M=modified, D=deleted, R=renamed
    lines_added     INTEGER,
    lines_removed   INTEGER
);

CREATE INDEX idx_cf_commit_hash ON commit_files(commit_hash);
```

---

## Derived Tables (Materialized Nightly)

### `feature_daily_stats`
Pre-aggregated performance stats per feature per day. Foundation for trend analysis and baselines.

```sql
CREATE TABLE IF NOT EXISTS feature_daily_stats (
    stat_date           DATE NOT NULL,
    feature_type        VARCHAR(16) NOT NULL,
    feature             VARCHAR(100) NOT NULL,
    event_count         INTEGER NOT NULL,
    distinct_users      INTEGER NOT NULL,
    distinct_workstations INTEGER NOT NULL,
    p50_ms              DOUBLE,              -- median duration (NULL if no durations)
    p95_ms              DOUBLE,
    p99_ms              DOUBLE,
    avg_ms              DOUBLE,
    total_wait_ms       BIGINT,              -- SUM(duration_ms) — for friction heatmap
    PRIMARY KEY (stat_date, feature_type, feature)
);
```

Rebuild SQL:
```sql
INSERT OR REPLACE INTO feature_daily_stats
SELECT
    CAST(mod_dt AS DATE)                        AS stat_date,
    feature_type,
    feature,
    COUNT(*)                                    AS event_count,
    COUNT(DISTINCT asmd_user)                   AS distinct_users,
    COUNT(DISTINCT workstation)                 AS distinct_workstations,
    PERCENTILE_CONT(0.50) WITHIN GROUP (ORDER BY duration_ms) FILTER (WHERE duration_ms IS NOT NULL) AS p50_ms,
    PERCENTILE_CONT(0.95) WITHIN GROUP (ORDER BY duration_ms) FILTER (WHERE duration_ms IS NOT NULL) AS p95_ms,
    PERCENTILE_CONT(0.99) WITHIN GROUP (ORDER BY duration_ms) FILTER (WHERE duration_ms IS NOT NULL) AS p99_ms,
    AVG(duration_ms) FILTER (WHERE duration_ms IS NOT NULL) AS avg_ms,
    SUM(duration_ms)                            AS total_wait_ms
FROM audit_events
GROUP BY CAST(mod_dt AS DATE), feature_type, feature;
```

---

### `feature_baselines`
Rolling 8-week baseline per feature. Used by Silent Degrader and Anomaly Guard.

```sql
CREATE TABLE IF NOT EXISTS feature_baselines (
    feature             VARCHAR(100) NOT NULL,
    feature_type        VARCHAR(16) NOT NULL,
    baseline_date       DATE NOT NULL,           -- date this baseline was computed
    window_weeks        INTEGER DEFAULT 8,
    mean_daily_count    DOUBLE,
    std_daily_count     DOUBLE,
    mean_p95_ms         DOUBLE,
    std_p95_ms          DOUBLE,
    mean_avg_ms         DOUBLE,
    std_avg_ms          DOUBLE,
    weekly_slope_ms     DOUBLE,                  -- linear regression slope (ms/week) for avg_ms
    weekly_slope_pct    DOUBLE,                  -- slope as % of mean_avg_ms per week
    PRIMARY KEY (feature, baseline_date)
);
```

---

### `user_sessions`
Sessions reconstructed from `audit_events`. A new session starts when a user has >30 minutes of inactivity.

```sql
CREATE TABLE IF NOT EXISTS user_sessions (
    session_id          BIGINT PRIMARY KEY,
    asmd_user           VARCHAR(20) NOT NULL,
    supp_user           VARCHAR(20) NOT NULL,
    workstation         VARCHAR(64) NOT NULL,
    session_start       TIMESTAMPTZ NOT NULL,
    session_end         TIMESTAMPTZ NOT NULL,
    event_count         INTEGER NOT NULL,
    is_impersonated     BOOLEAN NOT NULL,        -- supp_user != asmd_user for any event
    duration_minutes    DOUBLE
);
```

---

### `feature_sequences`
Consecutive feature transitions within sessions. Backbone of Workflow Discovery.

```sql
CREATE TABLE IF NOT EXISTS feature_sequences (
    from_feature        VARCHAR(100) NOT NULL,
    to_feature          VARCHAR(100) NOT NULL,
    from_feature_type   VARCHAR(16) NOT NULL,
    to_feature_type     VARCHAR(16) NOT NULL,
    transition_count    INTEGER NOT NULL,
    avg_gap_seconds     DOUBLE,                  -- avg time between the two actions
    PRIMARY KEY (from_feature, to_feature)
);
```

---

### `workstation_profiles`
Hardware/environment grouping derived from workstation ID patterns.

```sql
CREATE TABLE IF NOT EXISTS workstation_profiles (
    workstation         VARCHAR(64) PRIMARY KEY,
    office_prefix       VARCHAR(10),             -- e.g. LON, NYC, SIN (first 3 chars)
    last_seen           DATE,
    total_events        INTEGER,
    avg_startup_ms      DOUBLE,                  -- avg SYSTEM/Star Startup duration
    p95_startup_ms      DOUBLE
);
```

---

## Views

```sql
-- Support (impersonation) sessions only
CREATE OR REPLACE VIEW v_support_sessions AS
SELECT * FROM audit_events WHERE supp_user != asmd_user;

-- Normal (non-impersonated) sessions
CREATE OR REPLACE VIEW v_normal_sessions AS
SELECT * FROM audit_events WHERE supp_user = asmd_user;

-- Timed events only (exclude permission checks, etc.)
CREATE OR REPLACE VIEW v_timed_events AS
SELECT * FROM audit_events WHERE duration_ms IS NOT NULL AND duration_ms > 0;

-- Events enriched with nearest commit (by deployed_at)
CREATE OR REPLACE VIEW v_events_with_commit AS
SELECT
    e.*,
    c.commit_hash,
    c.tag             AS release_tag,
    c.deployed_at     AS deployment_time
FROM audit_events e
ASOF JOIN commits c ON e.mod_dt >= c.deployed_at   -- DuckDB ASOF join
ORDER BY e.mod_dt;
```

> **Note:** `ASOF JOIN` requires `commits.deployed_at` to be populated. If NULL (no deploy metadata), fallback to `committed_at`.

---

## Migration Strategy

Migrations are plain numbered SQL files in `src/db/migrations/`:

```
src/db/migrations/
  001_initial_schema.sql      -- raw tables
  002_derived_tables.sql      -- feature_daily_stats, baselines, sessions
  003_views.sql               -- all views
```

`src/db/connection.py` runs pending migrations on startup by tracking applied migrations in a `schema_migrations` table:

```sql
CREATE TABLE IF NOT EXISTS schema_migrations (
    version     INTEGER PRIMARY KEY,
    applied_at  TIMESTAMP DEFAULT now()
);
```
