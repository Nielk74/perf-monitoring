import { useState } from 'react'
import { useQuery } from '@tanstack/react-query'
import { api } from '../api/client'
import PageHeader from '../components/PageHeader'
import Chart from '../components/Chart'
import { PageSpinner } from '../components/Spinner'
import ErrorBanner from '../components/ErrorBanner'

const OFFICE_COLORS: Record<string, string> = { LON: '#6366f1', NYC: '#f97316', SIN: '#22c55e' }

export default function EnvironmentalFingerprint() {
  const [days, setDays] = useState(30)
  const { data: featuresMeta } = useQuery({ queryKey: ['features'], queryFn: api.features })
  const [featureType, setFeatureType] = useState('')

  const { data, isLoading, error } = useQuery({
    queryKey: ['envFingerprint', days, featureType],
    queryFn: () => api.envFingerprint(days, featureType || undefined),
  })

  const featureTypes = [...new Set(featuresMeta?.map(f => f.feature_type) ?? [])]

  // Pivot data: feature → {office → avg_ms}
  const rows = data?.data ?? []
  const features = [...new Set(rows.map(r => r.feature))]
  const offices = [...new Set(rows.map(r => r.office_prefix))].sort()

  const pivoted: Record<string, number | string>[] = features.map(feat => {
    const byOffice: Record<string, number> = {}
    rows.filter(r => r.feature === feat).forEach(r => { byOffice[r.office_prefix] = r.avg_ms })
    return { feature: feat, ...byOffice }
  })

  const barOption = {
    grid: { top: 8, right: 8, bottom: 40, left: 8, containLabel: true },
    xAxis: { type: 'category' as const, data: features, axisLabel: { rotate: 30, fontSize: 10, interval: 0 } },
    yAxis: { type: 'value' as const, name: 'avg ms', nameTextStyle: { color: '#737373', fontSize: 11 } },
    series: offices.map(office => ({
      name: office,
      type: 'bar' as const,
      data: pivoted.map(p => (p[office] as number) ?? null),
      itemStyle: { color: OFFICE_COLORS[office] ?? '#6366f1' },
      barMaxWidth: 24,
    })),
    legend: { bottom: 0, textStyle: { color: '#737373', fontSize: 11 } },
    tooltip: { trigger: 'axis' as const },
  }

  return (
    <div className="space-y-6 animate-slide-up">
      <PageHeader
        tag="UC-03"
        title="Environmental Fingerprint"
        description="Per-office latency comparison. Reveals infrastructure and network bottlenecks specific to each location."
        controls={
          <div className="flex items-center gap-2">
            <select className="select" value={featureType} onChange={e => setFeatureType(e.target.value)}>
              <option value="">All types</option>
              {featureTypes.map(t => <option key={t} value={t}>{t}</option>)}
            </select>
            <select className="select" value={days} onChange={e => setDays(+e.target.value)}>
              {[14, 30, 60, 90, 180, 365].map(v => <option key={v} value={v}>Last {v}d</option>)}
            </select>
          </div>
        }
      />

      {error && <ErrorBanner error={error} />}
      {isLoading && <PageSpinner />}

      {data && !isLoading && (
        <div className="space-y-4">
          {/* Grouped bar */}
          <div className="card p-5">
            <div className="label mb-4">Avg latency by office and feature</div>
            {features.length === 0
              ? <p className="text-sm text-secondary py-8 text-center">No data for current filters.</p>
              : <Chart option={barOption} height={300} />
            }
          </div>

          {/* Delta table */}
          <div className="card p-5">
            <div className="label mb-4">Office delta vs global average</div>
            <div className="overflow-auto">
              <table className="w-full text-sm">
                <thead>
                  <tr className="border-b border-border">
                    {['Feature', 'Type', 'Office', 'Avg ms', 'p95 ms', 'Δ vs global', 'Events'].map(h => (
                      <th key={h} className="text-left py-2 pr-4 label">{h}</th>
                    ))}
                  </tr>
                </thead>
                <tbody>
                  {rows.sort((a, b) => (b.delta_vs_global_pct ?? 0) - (a.delta_vs_global_pct ?? 0)).map((row, i) => (
                    <tr key={i} className="border-b border-border/50 table-row-hover">
                      <td className="py-2 pr-4 text-primary font-medium">{row.feature}</td>
                      <td className="py-2 pr-4 text-secondary text-xs">{row.feature_type}</td>
                      <td className="py-2 pr-4">
                        <span className="font-mono text-xs px-1.5 py-0.5 rounded" style={{ color: OFFICE_COLORS[row.office_prefix] ?? '#737373', background: `${OFFICE_COLORS[row.office_prefix] ?? '#737373'}18` }}>
                          {row.office_prefix}
                        </span>
                      </td>
                      <td className="py-2 pr-4 font-mono text-primary">{row.avg_ms?.toFixed(0)}</td>
                      <td className="py-2 pr-4 font-mono text-warn">{row.p95_ms?.toFixed(0)}</td>
                      <td className={`py-2 pr-4 font-mono ${(row.delta_vs_global_pct ?? 0) > 10 ? 'text-down' : (row.delta_vs_global_pct ?? 0) < -10 ? 'text-up' : 'text-secondary'}`}>
                        {row.delta_vs_global_pct != null ? `${row.delta_vs_global_pct > 0 ? '+' : ''}${row.delta_vs_global_pct.toFixed(1)}%` : '—'}
                      </td>
                      <td className="py-2 font-mono text-secondary">{row.event_count.toLocaleString()}</td>
                    </tr>
                  ))}
                </tbody>
              </table>
            </div>
          </div>
        </div>
      )}
    </div>
  )
}
