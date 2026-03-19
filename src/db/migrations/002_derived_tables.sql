-- Migration 002: derived tables (materialized nightly from raw data)

CREATE TABLE IF NOT EXISTS feature_daily_stats (
    stat_date               DATE NOT NULL,
    feature_type            VARCHAR(16) NOT NULL,
    feature                 VARCHAR(100) NOT NULL,
    event_count             INTEGER NOT NULL,
    distinct_users          INTEGER NOT NULL,
    distinct_workstations   INTEGER NOT NULL,
    p50_ms                  DOUBLE,
    p95_ms                  DOUBLE,
    p99_ms                  DOUBLE,
    avg_ms                  DOUBLE,
    total_wait_ms           BIGINT,
    PRIMARY KEY (stat_date, feature_type, feature)
);

CREATE TABLE IF NOT EXISTS feature_baselines (
    feature             VARCHAR(100) NOT NULL,
    feature_type        VARCHAR(16) NOT NULL,
    baseline_date       DATE NOT NULL,
    window_weeks        INTEGER DEFAULT 8,
    mean_daily_count    DOUBLE,
    std_daily_count     DOUBLE,
    mean_p95_ms         DOUBLE,
    std_p95_ms          DOUBLE,
    mean_avg_ms         DOUBLE,
    std_avg_ms          DOUBLE,
    weekly_slope_ms     DOUBLE,
    weekly_slope_pct    DOUBLE,
    PRIMARY KEY (feature, baseline_date)
);

CREATE TABLE IF NOT EXISTS user_sessions (
    session_id          BIGINT PRIMARY KEY,
    asmd_user           VARCHAR(20) NOT NULL,
    supp_user           VARCHAR(20) NOT NULL,
    workstation         VARCHAR(64) NOT NULL,
    session_start       TIMESTAMPTZ NOT NULL,
    session_end         TIMESTAMPTZ NOT NULL,
    event_count         INTEGER NOT NULL,
    is_impersonated     BOOLEAN NOT NULL,
    duration_minutes    DOUBLE
);

CREATE TABLE IF NOT EXISTS feature_sequences (
    from_feature        VARCHAR(100) NOT NULL,
    to_feature          VARCHAR(100) NOT NULL,
    from_feature_type   VARCHAR(16) NOT NULL,
    to_feature_type     VARCHAR(16) NOT NULL,
    transition_count    INTEGER NOT NULL,
    avg_gap_seconds     DOUBLE,
    PRIMARY KEY (from_feature, to_feature)
);

CREATE TABLE IF NOT EXISTS workstation_profiles (
    workstation         VARCHAR(64) PRIMARY KEY,
    office_prefix       VARCHAR(10),
    last_seen           DATE,
    total_events        INTEGER,
    avg_startup_ms      DOUBLE,
    p95_startup_ms      DOUBLE
);
