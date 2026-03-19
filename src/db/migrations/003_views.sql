-- Migration 003: views (lightweight SQL shortcuts)

CREATE OR REPLACE VIEW v_support_sessions AS
SELECT * FROM audit_events WHERE supp_user != asmd_user;

CREATE OR REPLACE VIEW v_normal_sessions AS
SELECT * FROM audit_events WHERE supp_user = asmd_user;

CREATE OR REPLACE VIEW v_timed_events AS
SELECT * FROM audit_events WHERE duration_ms IS NOT NULL AND duration_ms > 0;
