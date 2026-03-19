-- Migration 001: raw tables (direct imports, never modified after insert)

CREATE TABLE IF NOT EXISTS audit_events (
    id              BIGINT PRIMARY KEY,
    supp_user       VARCHAR(20) NOT NULL,
    asmd_user       VARCHAR(20) NOT NULL,
    workstation     VARCHAR(64) NOT NULL,
    mod_dt          TIMESTAMPTZ NOT NULL,
    feature_type    VARCHAR(16) NOT NULL,
    feature         VARCHAR(100) NOT NULL,
    detail          VARCHAR(100),
    duration_ms     INTEGER,
    imported_at     TIMESTAMP DEFAULT now()
);

CREATE INDEX IF NOT EXISTS idx_audit_mod_dt       ON audit_events(mod_dt);
CREATE INDEX IF NOT EXISTS idx_audit_asmd_user    ON audit_events(asmd_user);
CREATE INDEX IF NOT EXISTS idx_audit_feature_type ON audit_events(feature_type);
CREATE INDEX IF NOT EXISTS idx_audit_workstation  ON audit_events(workstation);

CREATE TABLE IF NOT EXISTS commits (
    commit_hash     VARCHAR(40) PRIMARY KEY,
    repo_name       VARCHAR(100) NOT NULL,
    author_name     VARCHAR(200),
    author_email    VARCHAR(200),
    committed_at    TIMESTAMP NOT NULL,
    message         TEXT,
    is_merge        BOOLEAN DEFAULT false,
    tag             VARCHAR(200),
    deployed_at     TIMESTAMP,
    imported_at     TIMESTAMP DEFAULT now()
);

CREATE INDEX IF NOT EXISTS idx_commits_committed_at ON commits(committed_at);
CREATE INDEX IF NOT EXISTS idx_commits_deployed_at  ON commits(deployed_at);

CREATE TABLE IF NOT EXISTS commit_files (
    commit_hash     VARCHAR(40) NOT NULL,
    file_path       VARCHAR(500) NOT NULL,
    change_type     VARCHAR(1),
    lines_added     INTEGER,
    lines_removed   INTEGER
);

CREATE INDEX IF NOT EXISTS idx_cf_commit_hash ON commit_files(commit_hash);
