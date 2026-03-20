import { useQuery } from '@tanstack/react-query'
import { api } from '../api/client'
import KpiCard from '../components/KpiCard'
import { PageSpinner } from '../components/Spinner'
import ErrorBanner from '../components/ErrorBanner'
import Chart from '../components/Chart'

function fmt(n: number | undefined) {
  if (n === undefined || n === null) return '—'
  return n >= 1_000_000 ? (n / 1_000_000).toFixed(1) + 'M'
    : n >= 1_000 ? (n / 1_000).toFixed(0) + 'K'
    : String(n)
}

function fmtDate(s: string | null | undefined) {
  if (!s) return '—'
  return new Date(s).toLocaleDateString('en-GB', { day: '2-digit', month: 'short', year: 'numeric' })
}

export default function Home() {
  const { data: dr, isLoading: drL } = useQuery({ queryKey: ['dateRange'], queryFn: api.dateRange })
  const { data: status, isLoading: stL } = useQuery({ queryKey: ['status'], queryFn: api.status })
  const { data: alerts } = useQuery({ queryKey: ['alertSummary', 2.5], queryFn: () => api.alertSummary(2.5) })
  const { data: daily, isLoading: dayL, error: dayErr } = useQuery({
    queryKey: ['dailyPressure', 60], queryFn: () => api.dailyPressure(60),
  })
  const { data: degraders } = useQuery({
    queryKey: ['silentDegrader', 1.0], queryFn: () => api.silentDegrader(1.0, 5),
  })

  const isLoading = drL || stL || dayL

  if (isLoading) return <PageSpinner />

  const totalAlerts = alerts
    ? (alerts.summary.latency_degraded ?? 0) + (alerts.summary.count_anomalies ?? 0)
    : 0

  const dailyOption = {
    grid: { top: 16, right: 8, bottom: 32, left: 52 },
    xAxis: {
      type: 'category' as const,
      data: daily?.data.map(d => d.stat_date.slice(5)) ?? [],
      axisLabel: { interval: 6 },
    },
    yAxis: { type: 'value' as const, name: 'Events', nameTextStyle: { color: '#737373', fontSize: 11 } },
    series: [{
      type: 'bar' as const,
      data: daily?.data.map(d => d.total_events) ?? [],
      itemStyle: { color: '#6366f1', opacity: 0.7 },
      emphasis: { itemStyle: { opacity: 1 } },
    }],
    tooltip: { trigger: 'axis' as const, formatter: (p: unknown[]) => {
      const pt = p[0] as { name: string; value: number }
      return `<b>${pt.name}</b><br/>${fmt(pt.value)} events`
    }},
  }

  return (
    <div className="space-y-6 animate-slide-up">
      {/* KPIs */}
      <div className="grid grid-cols-2 lg:grid-cols-4 gap-4">
        <KpiCard
          label="Total Audit Events"
          value={fmt(dr?.total_events)}
          sub={`Up to ${fmtDate(dr?.max_date)}`}
          accent
          icon={<svg viewBox="0 0 16 16" fill="none" className="w-4 h-4" stroke="currentColor" strokeWidth="1.5"><polyline points="1,12 5,7 9,9 15,3"/></svg>}
        />
        <KpiCard
          label="Date Range"
          value={fmtDate(dr?.min_date)}
          sub={`→ ${fmtDate(dr?.max_date)}`}
          icon={<svg viewBox="0 0 16 16" fill="none" className="w-4 h-4" stroke="currentColor" strokeWidth="1.5"><rect x="1.5" y="2.5" width="13" height="12" rx="1.5"/><line x1="5" y1="1" x2="5" y2="4"/><line x1="11" y1="1" x2="11" y2="4"/><line x1="1.5" y1="6.5" x2="14.5" y2="6.5"/></svg>}
        />
        <KpiCard
          label="Git Commits"
          value={fmt(status?.commits.row_count)}
          sub={`Last: ${fmtDate(status?.commits.last_committed_at)}`}
          icon={<svg viewBox="0 0 16 16" fill="none" className="w-4 h-4" stroke="currentColor" strokeWidth="1.5"><circle cx="8" cy="8" r="2.5"/><line x1="1" y1="8" x2="5.5" y2="8"/><line x1="10.5" y1="8" x2="15" y2="8"/></svg>}
        />
        <KpiCard
          label="Active Alerts"
          value={totalAlerts}
          sub={totalAlerts === 0 ? 'All clear' : `${alerts?.summary.latency_degraded} latency, ${alerts?.summary.count_anomalies} volume`}
          trend={totalAlerts > 0 ? 'down' : 'up'}
          icon={<svg viewBox="0 0 16 16" fill="none" className="w-4 h-4" stroke="currentColor" strokeWidth="1.5"><path d="M8 1.5 L14.5 13 H1.5 Z"/><line x1="8" y1="6" x2="8" y2="9.5"/><circle cx="8" cy="11.5" r="0.6" fill="currentColor" stroke="none"/></svg>}
        />
      </div>

      {/* Charts row */}
      <div className="grid grid-cols-1 lg:grid-cols-3 gap-4">
        {/* Daily event volume */}
        <div className="card p-5 lg:col-span-2">
          <div className="label mb-4">Daily Event Volume — 60 days</div>
          {dayErr ? <ErrorBanner error={dayErr} /> : <Chart option={dailyOption} height={240} />}
        </div>

        {/* Top degraders quick list */}
        <div className="card p-5">
          <div className="label mb-4">Top Degraders</div>
          {!degraders?.data.length ? (
            <p className="text-sm text-secondary">No degraders above threshold.</p>
          ) : (
            <div className="space-y-2">
              {degraders.data.slice(0, 5).map(d => (
                <div key={d.feature} className="flex items-center justify-between gap-2">
                  <span className="text-sm text-primary truncate">{d.feature}</span>
                  <span className="text-xs font-mono text-down flex-shrink-0">+{d.weekly_slope_pct.toFixed(1)}%/wk</span>
                </div>
              ))}
            </div>
          )}
        </div>
      </div>

      {/* System status */}
      <div className="card p-5">
        <div className="label mb-4">Ingestion Status</div>
        <div className="grid grid-cols-3 gap-6">
          {[
            { label: 'Audit Events', value: fmt(status?.audit_events.row_count), sub: status?.audit_events.max_mod_dt ? `Last: ${fmtDate(status.audit_events.max_mod_dt)}` : '—' },
            { label: 'Commits',      value: fmt(status?.commits.row_count),      sub: status?.commits.last_committed_at ? `Last: ${fmtDate(status.commits.last_committed_at)}` : '—' },
            { label: 'Materialized', value: fmtDate(status?.derived_tables.last_materialized), sub: 'Derived tables' },
          ].map(item => (
            <div key={item.label}>
              <div className="label mb-1">{item.label}</div>
              <div className="text-base font-semibold text-primary">{item.value}</div>
              <div className="text-xs text-secondary mt-0.5">{item.sub}</div>
            </div>
          ))}
        </div>
      </div>
    </div>
  )
}
