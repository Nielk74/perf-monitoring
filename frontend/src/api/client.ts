const BASE = import.meta.env.VITE_API_URL ?? ''

async function get<T>(path: string, params?: Record<string, string | number>): Promise<T> {
  const url = new URL(`${BASE}${path}`, window.location.origin)
  if (params) {
    Object.entries(params).forEach(([k, v]) => {
      if (v !== undefined && v !== null && v !== '') url.searchParams.set(k, String(v))
    })
  }
  const res = await fetch(url.toString())
  if (!res.ok) {
    const body = await res.json().catch(() => ({}))
    throw new Error(body.detail ?? `HTTP ${res.status}`)
  }
  return res.json()
}

// ── Types ────────────────────────────────────────────────────────────────────

export interface IngestionStatus {
  audit_events: { row_count: number; max_mod_dt: string | null }
  commits:      { row_count: number; last_committed_at: string | null }
  derived_tables: { last_materialized: string | null }
  jobs: Record<string, { status: string; last_run: string | null; last_result: unknown }>
}

export interface DateRange {
  min_date: string; max_date: string; total_events: number
}

export interface FeatureMeta {
  feature_type: string; feature: string; event_count: number
}

export interface Commit {
  commit_hash: string; repo_name: string; tag: string | null
  author_name: string; committed_at: string; deployed_at: string | null; message: string
}

// UC-01
export interface DegraderItem {
  feature: string; feature_type: string
  mean_avg_ms: number; mean_p95_ms: number
  weekly_slope_ms: number; weekly_slope_pct: number
  mean_daily_count: number; p95_last7: number | null
}
export interface DegraderResult { meta: Record<string, unknown>; data: DegraderItem[] }

// UC-02
export interface BlastItem {
  feature: string; feature_type: string
  before_avg_ms: number; after_avg_ms: number
  before_p95_ms: number; after_p95_ms: number
  delta_avg_ms: number; delta_pct: number; z_score: number
}
export interface BlastResult { meta: Record<string, unknown>; data: BlastItem[] }

// UC-03
export interface OfficeRow {
  office_prefix: string; feature: string; feature_type: string
  event_count: number; avg_ms: number; p95_ms: number; p50_ms: number
  delta_vs_global_pct: number | null
}

// UC-04
export interface SupportSession {
  session_id: number; asmd_user: string; supp_user: string
  workstation: string; session_start: string; session_end: string
  event_count: number; duration_minutes: number
}
export interface AgentSummary {
  supp_user: string; session_count: number; distinct_users_helped: number
  total_minutes: number; avg_session_minutes: number; last_session: string
}

// UC-05
export interface SunsetItem {
  feature: string; feature_type: string
  early_avg: number; recent_avg: number; peak_count: number
  last_seen: string; decline_pct: number
}

// UC-06
export interface FrictionItem {
  feature: string; feature_type: string
  total_events: number; total_wait_hours: number
  avg_ms: number; p95_ms: number; friction_score: number
}
export interface HourlyFriction {
  hour_utc: number; event_count: number; avg_ms: number; p95_ms: number
}

// UC-07
export interface TransitionRow {
  from_feature: string; to_feature: string
  from_feature_type: string; to_feature_type: string
  transition_count: number; avg_gap_seconds: number
}

// UC-08
export interface AdoptionWeek {
  week_start: string; event_count: number; weekly_users: number; cumulative_user_weeks: number
}

// UC-09
export interface PeakHourRow {
  day_of_week: number; hour_utc: number; event_count: number; distinct_users: number; avg_ms: number
}
export interface DailyPressure {
  stat_date: string; total_events: number; peak_daily_users: number
  avg_p95_ms: number; max_p99_ms: number; total_wait_hours: number
}

// UC-10
export interface AnomalyItem {
  feature: string; feature_type: string
  baseline_avg_ms: number; recent_avg_ms: number
  z_score: number; delta_pct: number
}

// ── API functions ─────────────────────────────────────────────────────────────

export const api = {
  // metadata
  dateRange:   () => get<DateRange>('/api/metadata/date-range'),
  features:    () => get<FeatureMeta[]>('/api/metadata/features'),
  offices:     () => get<string[]>('/api/metadata/offices'),
  commits:     (limit = 100) => get<Commit[]>('/api/metadata/commits', { limit }),

  // ingestion
  status:      () => get<IngestionStatus>('/api/ingestion/status'),
  triggerMat:  () => fetch('/api/ingestion/run/materialize', { method: 'POST' }),

  // UC-01
  silentDegrader: (slope = 1.0, topN = 20) =>
    get<DegraderResult>('/api/analytics/silent-degrader', { slope_threshold_pct: slope, top_n: topN }),
  featureTrend: (feature: string, days = 56) =>
    get<{ data: Record<string, unknown>[] }>('/api/analytics/silent-degrader/' + encodeURIComponent(feature) + '/trend', { days }),

  // UC-02
  deployments: () => get<{ data: Commit[] }>('/api/analytics/blast-radius/deployments'),
  blastRadius: (tag: string, windowDays = 14, zThreshold = 1.5) =>
    get<BlastResult>('/api/analytics/blast-radius', { tag, window_days: windowDays, z_threshold: zThreshold }),

  // UC-03
  envFingerprint: (days = 30, featureType?: string) =>
    get<{ data: OfficeRow[] }>('/api/analytics/environmental-fingerprint', { days, ...(featureType ? { feature_type: featureType } : {}) }),

  // UC-04
  impersonationSessions: (days = 90, limit = 100) =>
    get<{ data: SupportSession[] }>('/api/analytics/impersonation-audit', { days, limit }),
  supportAgents: (days = 90) =>
    get<{ data: AgentSummary[] }>('/api/analytics/impersonation-audit/agents', { days }),
  impersonationTimeline: (days = 90) =>
    get<{ data: Record<string, unknown>[] }>('/api/analytics/impersonation-audit/timeline', { days }),

  // UC-05
  featureSunset: (days = 90, declinePct = 30) =>
    get<{ data: SunsetItem[] }>('/api/analytics/feature-sunset', { days, decline_threshold_pct: declinePct }),
  zombieFeatures: () =>
    get<{ data: Record<string, unknown>[] }>('/api/analytics/feature-sunset/zombies'),
  usageTrend: (feature: string, days = 90) =>
    get<{ data: Record<string, unknown>[] }>('/api/analytics/feature-sunset/' + encodeURIComponent(feature) + '/trend', { days }),

  // UC-06
  frictionHeatmap: (days = 30, topN = 30) =>
    get<{ data: FrictionItem[] }>('/api/analytics/friction-heatmap', { days, top_n: topN }),
  hourlyFriction: (days = 30) =>
    get<{ data: HourlyFriction[] }>('/api/analytics/friction-heatmap/hourly', { days }),

  // UC-07
  transitions: (minCount = 5, topN = 50) =>
    get<{ data: TransitionRow[] }>('/api/analytics/workflow-discovery/transitions', { min_count: minCount, top_n: topN }),
  workflowGraph: (minCount = 10) =>
    get<{ nodes: unknown[]; edges: unknown[] }>('/api/analytics/workflow-discovery/graph', { min_count: minCount }),
  userGantt: (user: string, date: string) =>
    get<{ data: Record<string, unknown>[] }>('/api/analytics/workflow-discovery/gantt', { asmd_user: user, date }),

  // UC-08
  adoptionCurve: (feature: string, days = 180) =>
    get<{ data: AdoptionWeek[] }>('/api/analytics/adoption-velocity', { feature, days }),
  newFeatures: (days = 90) =>
    get<{ data: Record<string, unknown>[] }>('/api/analytics/adoption-velocity/new-features', { introduced_within_days: days }),

  // UC-09
  peakHours: (days = 30, office?: string) =>
    get<{ data: PeakHourRow[] }>('/api/analytics/peak-pressure', { days, ...(office ? { office } : {}) }),
  dailyPressure: (days = 90) =>
    get<{ data: DailyPressure[] }>('/api/analytics/peak-pressure/daily', { days }),
  officePeakOverlap: (days = 30) =>
    get<{ data: Record<string, unknown>[] }>('/api/analytics/peak-pressure/offices', { days }),

  // UC-10
  anomalies: (zThreshold = 2.5, lookback = 3) =>
    get<{ meta: Record<string, unknown>; data: AnomalyItem[] }>('/api/analytics/anomaly-guard', { z_threshold: zThreshold, lookback_days: lookback }),
  alertSummary: (zThreshold = 2.5) =>
    get<{ summary: Record<string, number>; latency_anomalies: AnomalyItem[]; count_anomalies: unknown[] }>(
      '/api/analytics/anomaly-guard/alert-summary', { z_threshold: zThreshold }
    ),
}
