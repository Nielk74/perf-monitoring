"""
src/api/routes/analytics.py

All 10 use-case analytics endpoints under /api/analytics/.
"""

from typing import Optional

from fastapi import APIRouter, Depends, HTTPException, Query
import duckdb

from src.api.deps import get_db
from src.analytics._db import rows
from src.analytics import (
    silent_degrader,
    blast_radius,
    environmental_fingerprint,
    impersonation_audit,
    feature_sunset,
    friction_heatmap,
    workflow_discovery,
    adoption_velocity,
    peak_pressure,
    anomaly_guard,
)

router = APIRouter(tags=["analytics"])


# ---------------------------------------------------------------------------
# UC-01 Silent Degrader
# ---------------------------------------------------------------------------

@router.get("/silent-degrader")
def get_silent_degrader(
    slope_threshold_pct: float = Query(1.0, description="Min weekly slope % to flag"),
    top_n: int = Query(20, ge=1, le=200),
    conn: duckdb.DuckDBPyConnection = Depends(get_db),
):
    return silent_degrader.get_silent_degraders(
        conn=conn,
        min_slope_pct=slope_threshold_pct,
        top_n=top_n,
    )


@router.get("/silent-degrader/{feature}/trend")
def get_feature_trend(
    feature: str,
    days: int = Query(56, ge=7, le=365),
    conn: duckdb.DuckDBPyConnection = Depends(get_db),
):
    return silent_degrader.get_feature_trend(feature=feature, conn=conn, days=days)


# ---------------------------------------------------------------------------
# UC-02 Blast Radius
# ---------------------------------------------------------------------------

@router.get("/blast-radius/deployments")
def list_deployments(
    limit: int = Query(100, ge=1, le=500),
    conn: duckdb.DuckDBPyConnection = Depends(get_db),
):
    return blast_radius.list_deployments(conn=conn, limit=limit)


@router.get("/blast-radius/summaries")
def get_deployment_summaries(
    from_date: str = Query(..., description="Start date YYYY-MM-DD"),
    to_date: str = Query(..., description="End date YYYY-MM-DD"),
    window_days: int = Query(14, ge=3, le=60),
    z_threshold: float = Query(2.0, ge=0.5),
    conn: duckdb.DuckDBPyConnection = Depends(get_db),
):
    return blast_radius.get_deployment_summaries(
        from_date=from_date, to_date=to_date,
        conn=conn, window_days=window_days, z_threshold=z_threshold,
    )


@router.get("/blast-radius/hot-files")
def get_hot_files(
    window_days: int = Query(14, ge=3, le=90),
    regression_pct: float = Query(10.0, ge=1.0),
    top_n: int = Query(30, ge=1, le=100),
    conn: duckdb.DuckDBPyConnection = Depends(get_db),
):
    return blast_radius.get_hot_files(
        conn=conn, window_days=window_days, regression_pct=regression_pct, top_n=top_n
    )


@router.get("/blast-radius/author-impact")
def get_author_impact(
    window_days: int = Query(14, ge=3, le=90),
    regression_pct: float = Query(10.0, ge=1.0),
    top_n: int = Query(20, ge=1, le=100),
    conn: duckdb.DuckDBPyConnection = Depends(get_db),
):
    return blast_radius.get_author_impact(
        conn=conn, window_days=window_days, regression_pct=regression_pct, top_n=top_n
    )


@router.get("/blast-radius")
def get_blast_radius(
    deployed_date: str = Query(..., description="Deployment date YYYY-MM-DD"),
    window_days: int = Query(14, ge=3, le=90),
    z_threshold: float = Query(2.0, ge=0.5),
    conn: duckdb.DuckDBPyConnection = Depends(get_db),
):
    return blast_radius.get_blast_radius(
        deployed_date=deployed_date,
        conn=conn,
        window_days=window_days,
        z_threshold=z_threshold,
    )


@router.get("/blast-radius/{deployed_date}/diff")
def get_deployment_diff(
    deployed_date: str,
    conn: duckdb.DuckDBPyConnection = Depends(get_db),
):
    return blast_radius.get_deployment_diff(deployed_date=deployed_date, conn=conn)


# ---------------------------------------------------------------------------
# UC-03 Environmental Fingerprint
# ---------------------------------------------------------------------------

@router.get("/environmental-fingerprint")
def get_env_fingerprint(
    days: int = Query(30, ge=7, le=365),
    feature_type: Optional[str] = None,
    min_events: int = Query(20, ge=1),
    conn: duckdb.DuckDBPyConnection = Depends(get_db),
):
    return environmental_fingerprint.get_office_comparison(
        conn=conn,
        days=days,
        min_events=min_events,
        feature_type=feature_type,
    )


@router.get("/environmental-fingerprint/workstations")
def get_workstation_outliers(
    days: int = Query(30, ge=7, le=365),
    min_events: int = Query(50, ge=1),
    top_n: int = Query(20, ge=1, le=100),
    conn: duckdb.DuckDBPyConnection = Depends(get_db),
):
    return environmental_fingerprint.get_workstation_outliers(
        conn=conn, days=days, min_events=min_events, top_n=top_n
    )


# ---------------------------------------------------------------------------
# UC-04 Impersonation Audit
# ---------------------------------------------------------------------------

@router.get("/impersonation-audit")
def get_impersonation_audit(
    days: int = Query(90, ge=1, le=365),
    supp_user: Optional[str] = None,
    asmd_user: Optional[str] = None,
    limit: int = Query(200, ge=1, le=1000),
    conn: duckdb.DuckDBPyConnection = Depends(get_db),
):
    return impersonation_audit.get_support_sessions(
        conn=conn,
        days=days,
        supp_user=supp_user,
        asmd_user=asmd_user,
        limit=limit,
    )


@router.get("/impersonation-audit/agents")
def get_support_agents(
    days: int = Query(90, ge=1, le=365),
    conn: duckdb.DuckDBPyConnection = Depends(get_db),
):
    return impersonation_audit.get_support_agent_summary(conn=conn, days=days)


@router.get("/impersonation-audit/timeline")
def get_impersonation_timeline(
    days: int = Query(90, ge=1, le=365),
    conn: duckdb.DuckDBPyConnection = Depends(get_db),
):
    return impersonation_audit.get_impersonation_timeline(conn=conn, days=days)


@router.get("/impersonation-audit/features")
def get_support_features(
    days: int = Query(90, ge=1, le=365),
    top_n: int = Query(30, ge=1, le=100),
    conn: duckdb.DuckDBPyConnection = Depends(get_db),
):
    return impersonation_audit.get_features_accessed_during_support(
        conn=conn, days=days, top_n=top_n
    )


# ---------------------------------------------------------------------------
# UC-05 Feature Sunset
# ---------------------------------------------------------------------------

@router.get("/feature-sunset")
def get_feature_sunset(
    days: int = Query(90, ge=30, le=730),
    min_peak_events: int = Query(10, ge=1),
    decline_threshold_pct: float = Query(50.0, ge=10.0),
    top_n: int = Query(30, ge=1, le=100),
    conn: duckdb.DuckDBPyConnection = Depends(get_db),
):
    return feature_sunset.get_sunset_candidates(
        conn=conn,
        days=days,
        min_peak_events=min_peak_events,
        decline_threshold_pct=decline_threshold_pct,
        top_n=top_n,
    )


@router.get("/feature-sunset/zombies")
def get_zombie_features(
    inactive_days: int = Query(30, ge=7, le=365),
    min_total_events: int = Query(5, ge=1),
    conn: duckdb.DuckDBPyConnection = Depends(get_db),
):
    return feature_sunset.get_zombie_features(
        conn=conn, inactive_days=inactive_days, min_total_events=min_total_events
    )


@router.get("/feature-sunset/{feature}/trend")
def get_usage_trend(
    feature: str,
    days: int = Query(90, ge=7, le=365),
    conn: duckdb.DuckDBPyConnection = Depends(get_db),
):
    return feature_sunset.get_usage_trend(feature=feature, conn=conn, days=days)


# ---------------------------------------------------------------------------
# UC-06 Friction Heatmap
# ---------------------------------------------------------------------------

@router.get("/friction-heatmap")
def get_friction_heatmap(
    days: int = Query(30, ge=7, le=365),
    min_events: int = Query(10, ge=1),
    top_n: int = Query(40, ge=1, le=100),
    conn: duckdb.DuckDBPyConnection = Depends(get_db),
):
    return friction_heatmap.get_friction_heatmap(
        conn=conn, days=days, min_events=min_events, top_n=top_n
    )


@router.get("/friction-heatmap/hourly")
def get_hourly_friction(
    days: int = Query(30, ge=7, le=365),
    feature_type: Optional[str] = None,
    conn: duckdb.DuckDBPyConnection = Depends(get_db),
):
    return friction_heatmap.get_hourly_friction(
        conn=conn, days=days, feature_type=feature_type
    )


@router.get("/friction-heatmap/users")
def get_user_friction(
    days: int = Query(30, ge=7, le=365),
    top_n: int = Query(20, ge=1, le=100),
    conn: duckdb.DuckDBPyConnection = Depends(get_db),
):
    return friction_heatmap.get_user_friction(conn=conn, days=days, top_n=top_n)


# ---------------------------------------------------------------------------
# UC-07 Workflow Discovery
# ---------------------------------------------------------------------------

@router.get("/workflow-discovery/transitions")
def get_transitions(
    from_feature: Optional[str] = None,
    to_feature: Optional[str] = None,
    feature_type: Optional[str] = None,
    top_n: int = Query(50, ge=1, le=200),
    min_count: int = Query(5, ge=1),
    conn: duckdb.DuckDBPyConnection = Depends(get_db),
):
    return workflow_discovery.get_top_transitions(
        conn=conn,
        from_feature=from_feature,
        to_feature=to_feature,
        feature_type=feature_type,
        top_n=top_n,
        min_count=min_count,
    )


@router.get("/workflow-discovery/graph")
def get_workflow_graph(
    min_count: int = Query(10, ge=1),
    max_edges: int = Query(100, ge=10, le=500),
    conn: duckdb.DuckDBPyConnection = Depends(get_db),
):
    return workflow_discovery.get_workflow_graph(
        conn=conn, min_count=min_count, max_edges=max_edges
    )


@router.get("/workflow-discovery/gantt")
def get_user_gantt(
    asmd_user: str = Query(..., description="User to show timeline for"),
    date: str = Query(..., description="Date (YYYY-MM-DD)"),
    conn: duckdb.DuckDBPyConnection = Depends(get_db),
):
    """Feature usage timeline for a single user on a given day (for gantt chart)."""
    cur = conn.execute("""
        SELECT
            mod_dt,
            feature_type,
            feature,
            duration_ms,
            workstation,
            supp_user
        FROM audit_events
        WHERE asmd_user = ?
          AND CAST(mod_dt AS DATE) = CAST(? AS DATE)
        ORDER BY mod_dt
    """, [asmd_user, date])
    data = rows(cur)
    return {
        "meta": {"use_case": "UC-07", "asmd_user": asmd_user, "date": date},
        "data": data,
    }


@router.get("/workflow-discovery/sessions")
def get_session_summary(
    days: int = Query(30, ge=7, le=365),
    conn: duckdb.DuckDBPyConnection = Depends(get_db),
):
    return workflow_discovery.get_session_summary(conn=conn, days=days)


# ---------------------------------------------------------------------------
# UC-08 Adoption Velocity
# ---------------------------------------------------------------------------

@router.get("/adoption-velocity")
def get_adoption_velocity(
    feature: str = Query(..., description="Feature name to track"),
    days: int = Query(180, ge=14, le=730),
    conn: duckdb.DuckDBPyConnection = Depends(get_db),
):
    return adoption_velocity.get_adoption_curve(feature=feature, conn=conn, days=days)


@router.get("/adoption-velocity/new-features")
def get_new_features(
    introduced_within_days: int = Query(90, ge=7, le=365),
    min_events: int = Query(5, ge=1),
    conn: duckdb.DuckDBPyConnection = Depends(get_db),
):
    return adoption_velocity.get_new_features(
        conn=conn,
        introduced_within_days=introduced_within_days,
        min_events=min_events,
    )


@router.get("/adoption-velocity/by-type")
def get_type_adoption(
    days: int = Query(90, ge=14, le=365),
    conn: duckdb.DuckDBPyConnection = Depends(get_db),
):
    return adoption_velocity.get_feature_type_adoption(conn=conn, days=days)


# ---------------------------------------------------------------------------
# UC-09 Peak Pressure
# ---------------------------------------------------------------------------

@router.get("/peak-pressure")
def get_peak_pressure(
    days: int = Query(30, ge=7, le=365),
    office: Optional[str] = Query(None, description="Workstation prefix (LON/NYC/SIN)"),
    conn: duckdb.DuckDBPyConnection = Depends(get_db),
):
    return peak_pressure.get_peak_hours(conn=conn, days=days, office_prefix=office)


@router.get("/peak-pressure/daily")
def get_daily_pressure(
    days: int = Query(90, ge=7, le=365),
    conn: duckdb.DuckDBPyConnection = Depends(get_db),
):
    return peak_pressure.get_daily_pressure(conn=conn, days=days)


@router.get("/peak-pressure/offices")
def get_office_overlap(
    days: int = Query(30, ge=7, le=365),
    conn: duckdb.DuckDBPyConnection = Depends(get_db),
):
    return peak_pressure.get_office_peak_overlap(conn=conn, days=days)


# ---------------------------------------------------------------------------
# UC-10 Anomaly Guard
# ---------------------------------------------------------------------------

@router.get("/anomaly-guard")
def get_anomaly_guard(
    lookback_days: int = Query(3, ge=1, le=30),
    z_threshold: float = Query(3.0, ge=1.0),
    min_events: int = Query(5, ge=1),
    conn: duckdb.DuckDBPyConnection = Depends(get_db),
):
    return anomaly_guard.get_anomalies(
        conn=conn,
        lookback_days=lookback_days,
        z_threshold=z_threshold,
        min_events=min_events,
    )


@router.get("/anomaly-guard/count-anomalies")
def get_count_anomalies(
    lookback_days: int = Query(3, ge=1, le=30),
    z_threshold: float = Query(3.0, ge=1.0),
    conn: duckdb.DuckDBPyConnection = Depends(get_db),
):
    return anomaly_guard.get_count_anomalies(
        conn=conn, lookback_days=lookback_days, z_threshold=z_threshold
    )


@router.get("/anomaly-guard/alert-summary")
def get_alert_summary(
    lookback_days: int = Query(1, ge=1, le=7),
    z_threshold: float = Query(2.5, ge=1.0),
    conn: duckdb.DuckDBPyConnection = Depends(get_db),
):
    return anomaly_guard.get_alert_summary(
        conn=conn, lookback_days=lookback_days, z_threshold=z_threshold
    )
