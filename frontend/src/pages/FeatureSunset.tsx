import { useState } from 'react'
import { useQuery } from '@tanstack/react-query'
import { api } from '../api/client'
import PageHeader from '../components/PageHeader'
import Chart from '../components/Chart'
import { PageSpinner } from '../components/Spinner'
import ErrorBanner from '../components/ErrorBanner'

export default function FeatureSunset() {
  const [days, setDays] = useState(90)
  const [decline, setDecline] = useState(30)
  const [selected, setSelected] = useState<string | null>(null)

  const { data, isLoading, error } = useQuery({
    queryKey: ['featureSunset', days, decline],
    queryFn: () => api.featureSunset(days, decline),
  })
  const { data: zombies } = useQuery({ queryKey: ['zombies'], queryFn: api.zombieFeatures })
  const { data: trend } = useQuery({
    queryKey: ['usageTrend', selected, days],
    queryFn: () => api.usageTrend(selected!, days),
    enabled: !!selected,
  })

  const barOption = data && {
    grid: { top: 8, right: 80, bottom: 8, left: 8, containLabel: true },
    xAxis: { type: 'value' as const, axisLabel: { formatter: (v: number) => `${v.toFixed(0)}%` } },
    yAxis: { type: 'category' as const, data: [...data.data].reverse().map(d => d.feature), axisLabel: { fontSize: 11 } },
    series: [{
      type: 'bar' as const,
      data: [...data.data].reverse().map(d => ({ value: +d.decline_pct.toFixed(1), itemStyle: { color: '#ef4444' } })),
      barMaxWidth: 24,
      label: { show: true, position: 'right' as const, formatter: (p: { value: number }) => `${p.value.toFixed(0)}%`, color: '#737373', fontSize: 10 },
    }],
    tooltip: { trigger: 'axis' as const, formatter: (p: unknown[]) => {
      const pt = (p as { name: string; value: number }[])[0]
      return `<b>${pt.name}</b><br/>Decline: ${pt.value.toFixed(1)}%`
    }},
  }

  const trendOption = trend && {
    grid: { top: 12, right: 8, bottom: 28, left: 48 },
    xAxis: { type: 'category' as const, data: trend.data.map((d: Record<string, unknown>) => String(d.stat_date ?? '').slice(5)) },
    yAxis: { type: 'value' as const, name: 'Events/day', nameTextStyle: { color: '#737373', fontSize: 11 } },
    series: [{
      type: 'line' as const, smooth: true, showSymbol: false,
      data: trend.data.map((d: Record<string, unknown>) => d.event_count as number),
      lineStyle: { color: '#ef4444' }, areaStyle: { color: 'rgba(239,68,68,0.06)' }, itemStyle: { color: '#ef4444' },
    }],
    tooltip: { trigger: 'axis' as const },
  }

  return (
    <div className="space-y-6 animate-slide-up">
      <PageHeader
        tag="UC-05"
        title="Feature Sunset"
        description="Features with significant usage decline — candidates for archival or removal."
        controls={
          <div className="flex items-center gap-2">
            <select className="select" value={days} onChange={e => setDays(+e.target.value)}>
              {[30, 60, 90, 180, 365].map(v => <option key={v} value={v}>Last {v}d</option>)}
            </select>
            <select className="select" value={decline} onChange={e => setDecline(+e.target.value)}>
              {[20, 30, 50, 70].map(v => <option key={v} value={v}>Decline ≥ {v}%</option>)}
            </select>
          </div>
        }
      />

      {error && <ErrorBanner error={error} />}
      {isLoading && <PageSpinner />}

      {data && !isLoading && (
        <div className="grid grid-cols-1 lg:grid-cols-5 gap-4">
          <div className="card p-5 lg:col-span-2">
            <div className="label mb-4">Usage decline — {data.data.length} features</div>
            {data.data.length === 0
              ? <p className="text-sm text-secondary py-8 text-center">No features above {decline}% decline.</p>
              : barOption && <Chart option={barOption} height={Math.max(180, data.data.length * 30 + 32)} />
            }

            {/* Zombies */}
            <div className="mt-6 pt-4 border-t border-border">
              <div className="label mb-3">Zombie features ({zombies?.data.length ?? 0})</div>
              <div className="space-y-1.5">
                {zombies?.data.map((z: Record<string, unknown>, i: number) => (
                  <div key={i} className="flex items-center justify-between gap-2 text-sm">
                    <span className="text-secondary truncate">{String(z.feature)}</span>
                    <span className="text-xs font-mono text-muted flex-shrink-0">{String(z.days_silent)}d ago</span>
                  </div>
                ))}
              </div>
            </div>
          </div>

          <div className="card p-5 lg:col-span-3">
            <div className="label mb-4">Click a row to see its trend</div>
            <table className="w-full text-sm">
              <thead>
                <tr className="border-b border-border">
                  {['Feature', 'Type', 'Early avg', 'Recent avg', 'Decline', 'Last seen'].map(h => (
                    <th key={h} className="text-left py-2 pr-3 label text-xs">{h}</th>
                  ))}
                </tr>
              </thead>
              <tbody>
                {data.data.map(row => (
                  <tr
                    key={row.feature}
                    onClick={() => setSelected(row.feature)}
                    className={`border-b border-border/50 cursor-pointer table-row-hover ${selected === row.feature ? 'bg-accent/5' : ''}`}
                  >
                    <td className="py-2 pr-3 text-primary font-medium">{row.feature}</td>
                    <td className="py-2 pr-3 text-secondary text-xs">{row.feature_type}</td>
                    <td className="py-2 pr-3 font-mono text-secondary">{row.early_avg?.toFixed(1)}</td>
                    <td className="py-2 pr-3 font-mono text-primary">{row.recent_avg?.toFixed(1)}</td>
                    <td className="py-2 pr-3 font-mono text-down">{row.decline_pct?.toFixed(0)}%</td>
                    <td className="py-2 text-secondary text-xs">{String(row.last_seen).slice(0, 10)}</td>
                  </tr>
                ))}
              </tbody>
            </table>

            {selected && trendOption && (
              <div className="mt-4 pt-4 border-t border-border">
                <div className="label mb-3">Usage trend: {selected}</div>
                <Chart option={trendOption} height={180} />
              </div>
            )}
          </div>
        </div>
      )}
    </div>
  )
}
