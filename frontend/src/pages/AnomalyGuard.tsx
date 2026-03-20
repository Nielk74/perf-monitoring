import { useState } from 'react'
import { useQuery } from '@tanstack/react-query'
import { api } from '../api/client'
import PageHeader from '../components/PageHeader'
import Chart from '../components/Chart'
import { PageSpinner } from '../components/Spinner'
import ErrorBanner from '../components/ErrorBanner'

export default function AnomalyGuard() {
  const [zThreshold, setZThreshold] = useState(2.0)
  const [lookback, setLookback] = useState(3)

  const { data, isLoading, error } = useQuery({
    queryKey: ['anomalies', zThreshold, lookback],
    queryFn: () => api.anomalies(zThreshold, lookback),
  })
  const { data: alerts } = useQuery({
    queryKey: ['alertSummary', zThreshold],
    queryFn: () => api.alertSummary(zThreshold),
  })

  const scatterOption = data && data.data.length > 0 && {
    grid: { top: 16, right: 24, bottom: 40, left: 60 },
    xAxis: { type: 'value' as const, name: 'Δ% vs baseline', nameTextStyle: { color: '#737373', fontSize: 11 }, axisLabel: { formatter: (v: number) => `${v > 0 ? '+' : ''}${v.toFixed(0)}%` } },
    yAxis: { type: 'value' as const, name: 'z-score', nameTextStyle: { color: '#737373', fontSize: 11 } },
    series: [{
      type: 'scatter' as const,
      data: data.data.map(d => [d.delta_pct, d.z_score, d.feature]),
      symbolSize: 10,
      itemStyle: {
        color: (p: { value: [number, number, string] }) =>
          p.value[1] > 3 ? '#ef4444' : p.value[1] > 2 ? '#f59e0b' : '#6366f1',
        opacity: 0.85,
      },
      label: { show: true, formatter: (p: { value: [number, number, string] }) => p.value[2], position: 'top' as const, fontSize: 10, color: '#737373' },
    }],
    tooltip: { trigger: 'item' as const, formatter: (p: { value: [number, number, string] }) =>
      `<b>${p.value[2]}</b><br/>Δ: ${p.value[0] > 0 ? '+' : ''}${p.value[0].toFixed(1)}%<br/>z-score: ${p.value[1].toFixed(2)}`
    },
    markLine: {
      data: [{ yAxis: zThreshold, lineStyle: { color: '#f59e0b', type: 'dashed' } }],
      label: { formatter: `z = ${zThreshold}`, color: '#f59e0b', fontSize: 10 },
    },
  }

  const degraded = data?.data.filter(d => d.z_score > 0) ?? []
  const improved  = data?.data.filter(d => d.z_score < 0) ?? []

  return (
    <div className="space-y-6 animate-slide-up">
      <PageHeader
        tag="UC-10"
        title="Anomaly Guard"
        description="Z-score anomaly detection: compares recent latency against the 8-week baseline per feature."
        controls={
          <div className="flex items-center gap-2">
            <label className="flex items-center gap-2 text-sm text-secondary">
              Lookback
              <select className="select" value={lookback} onChange={e => setLookback(+e.target.value)}>
                {[1, 3, 7].map(v => <option key={v} value={v}>{v}d</option>)}
              </select>
            </label>
            <label className="flex items-center gap-2 text-sm text-secondary">
              z ≥
              <select className="select" value={zThreshold} onChange={e => setZThreshold(+e.target.value)}>
                {[1.5, 2.0, 2.5, 3.0].map(v => <option key={v} value={v}>{v}</option>)}
              </select>
            </label>
          </div>
        }
      />

      {error && <ErrorBanner error={error} />}
      {isLoading && <PageSpinner />}

      {/* Alert banner */}
      {alerts && alerts.summary.latency_degraded > 0 && (
        <div className="flex items-center gap-3 px-4 py-3 rounded-lg border border-down/30 bg-down/5 animate-slide-up">
          <svg viewBox="0 0 16 16" fill="none" className="w-4 h-4 text-down flex-shrink-0" stroke="currentColor" strokeWidth="1.5">
            <path d="M8 1.5 L14.5 13 H1.5 Z"/><line x1="8" y1="6" x2="8" y2="9.5"/>
            <circle cx="8" cy="11.5" r="0.6" fill="currentColor" stroke="none"/>
          </svg>
          <span className="text-sm text-down">
            <strong>{alerts.summary.latency_degraded}</strong> feature{alerts.summary.latency_degraded !== 1 ? 's' : ''} showing latency degradation above z≥{zThreshold}
          </span>
        </div>
      )}
      {alerts && alerts.summary.latency_degraded === 0 && alerts.summary.count_anomalies === 0 && (
        <div className="flex items-center gap-3 px-4 py-3 rounded-lg border border-up/30 bg-up/5 animate-slide-up">
          <svg viewBox="0 0 16 16" fill="none" className="w-4 h-4 text-up flex-shrink-0" stroke="currentColor" strokeWidth="1.5">
            <path d="M2 8 L6.5 12.5 L14 4"/><circle cx="8" cy="8" r="7"/>
          </svg>
          <span className="text-sm text-up">All clear — no anomalies above z≥{zThreshold} in the last {lookback} day{lookback !== 1 ? 's' : ''}.</span>
        </div>
      )}

      {data && !isLoading && (
        <div className="space-y-4">
          {/* KPIs */}
          <div className="grid grid-cols-3 gap-4">
            {[
              { label: 'Degraded features', value: degraded.length, color: degraded.length > 0 ? 'text-down' : 'text-primary' },
              { label: 'Improved features', value: improved.length, color: 'text-up' },
              { label: 'Max z-score', value: data.data.length ? Math.max(...data.data.map(d => Math.abs(d.z_score))).toFixed(2) : '—', color: 'text-warn' },
            ].map(kpi => (
              <div key={kpi.label} className="card p-4">
                <div className="label mb-1">{kpi.label}</div>
                <div className={`text-2xl font-semibold ${kpi.color}`}>{kpi.value}</div>
              </div>
            ))}
          </div>

          <div className="grid grid-cols-1 lg:grid-cols-5 gap-4">
            {/* Scatter */}
            <div className="card p-5 lg:col-span-3">
              <div className="label mb-4">z-score vs Δ% — {data.data.length} anomalies</div>
              {!scatterOption
                ? <p className="text-sm text-secondary py-12 text-center">No anomalies detected at z≥{zThreshold}.</p>
                : <Chart option={scatterOption} height={300} />
              }
            </div>

            {/* Table */}
            <div className="card p-5 lg:col-span-2">
              <div className="label mb-4">Anomaly details</div>
              {data.data.length === 0
                ? <p className="text-sm text-secondary py-4">No anomalies for current settings.</p>
                : (
                  <div className="overflow-auto max-h-72">
                    <table className="w-full text-sm">
                      <thead className="sticky top-0 bg-raised">
                        <tr className="border-b border-border">
                          {['Feature', 'z-score', 'Δ%', 'Baseline', 'Recent'].map(h => (
                            <th key={h} className="text-left py-2 pr-3 label text-xs">{h}</th>
                          ))}
                        </tr>
                      </thead>
                      <tbody>
                        {data.data.map(row => (
                          <tr key={row.feature} className="border-b border-border/50 table-row-hover">
                            <td className="py-2 pr-3 text-primary font-medium text-xs">{row.feature}</td>
                            <td className={`py-2 pr-3 font-mono text-xs ${row.z_score > 0 ? 'text-down' : 'text-up'}`}>
                              {row.z_score > 0 ? '+' : ''}{row.z_score?.toFixed(2)}
                            </td>
                            <td className={`py-2 pr-3 font-mono text-xs ${row.delta_pct > 0 ? 'text-down' : 'text-up'}`}>
                              {row.delta_pct > 0 ? '+' : ''}{row.delta_pct?.toFixed(1)}%
                            </td>
                            <td className="py-2 pr-3 font-mono text-secondary text-xs">{row.baseline_avg_ms?.toFixed(0)}</td>
                            <td className="py-2 font-mono text-primary text-xs">{row.recent_avg_ms?.toFixed(0)}</td>
                          </tr>
                        ))}
                      </tbody>
                    </table>
                  </div>
                )
              }
            </div>
          </div>
        </div>
      )}
    </div>
  )
}
