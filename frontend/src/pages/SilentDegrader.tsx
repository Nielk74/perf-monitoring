import { useState } from 'react'
import { useQuery } from '@tanstack/react-query'
import { api, type DegraderItem } from '../api/client'
import PageHeader from '../components/PageHeader'
import Chart from '../components/Chart'
import { PageSpinner } from '../components/Spinner'
import ErrorBanner from '../components/ErrorBanner'

export default function SilentDegrader() {
  const [slope, setSlope] = useState(0.5)
  const [selected, setSelected] = useState<DegraderItem | null>(null)

  const { data, isLoading, error } = useQuery({
    queryKey: ['silentDegrader', slope],
    queryFn: () => api.silentDegrader(slope, 20),
  })
  const { data: trend } = useQuery({
    queryKey: ['featureTrend', selected?.feature],
    queryFn: () => api.featureTrend(selected!.feature, 56),
    enabled: !!selected,
  })

  const trendOption = {
    grid: { top: 12, right: 8, bottom: 36, left: 52 },
    xAxis: { type: 'category' as const, data: trend?.data.map((d: Record<string, unknown>) => String(d.stat_date ?? '').slice(5)) ?? [] },
    yAxis: { type: 'value' as const, name: 'ms', nameTextStyle: { color: '#737373', fontSize: 11 } },
    series: [
      { name: 'avg', type: 'line' as const, data: trend?.data.map((d: Record<string, unknown>) => d.avg_ms as number), smooth: true, lineStyle: { color: '#6366f1' }, itemStyle: { color: '#6366f1' }, showSymbol: false },
      { name: 'p95', type: 'line' as const, data: trend?.data.map((d: Record<string, unknown>) => d.p95_ms as number), smooth: true, lineStyle: { color: '#ef4444', type: 'dashed' }, itemStyle: { color: '#ef4444' }, showSymbol: false },
    ],
    legend: { data: ['avg', 'p95'], bottom: 0, textStyle: { color: '#737373', fontSize: 11 } },
    tooltip: { trigger: 'axis' as const },
  }

  const slopeBarOption = data && {
    grid: { top: 8, right: 60, bottom: 8, left: 8, containLabel: true },
    xAxis: { type: 'value' as const, axisLabel: { formatter: (v: number) => `+${v.toFixed(1)}%` } },
    yAxis: { type: 'category' as const, data: [...(data.data ?? [])].reverse().map(d => d.feature), axisLabel: { fontSize: 11, width: 120, overflow: 'truncate' } },
    series: [{
      type: 'bar' as const,
      data: [...(data.data ?? [])].reverse().map(d => ({
        value: d.weekly_slope_pct,
        itemStyle: { color: d.weekly_slope_pct > 5 ? '#ef4444' : d.weekly_slope_pct > 2 ? '#f59e0b' : '#6366f1' },
      })),
      barMaxWidth: 28,
    }],
    tooltip: { trigger: 'axis' as const, formatter: (p: unknown[]) => {
      const pt = (p as { name: string; value: number }[])[0]
      return `<b>${pt.name}</b><br/>+${pt.value.toFixed(2)}%/week`
    }},
  }

  return (
    <div className="space-y-6 animate-slide-up">
      <PageHeader
        tag="UC-01"
        title="Silent Degrader"
        description="Features with a significant positive weekly latency slope over the past 8 weeks."
        controls={
          <label className="flex items-center gap-2 text-sm text-secondary">
            Min slope
            <select className="select" value={slope} onChange={e => setSlope(+e.target.value)}>
              {[0.5, 1, 2, 5].map(v => <option key={v} value={v}>{v}%/wk</option>)}
            </select>
          </label>
        }
      />

      {error && <ErrorBanner error={error} />}
      {isLoading && <PageSpinner />}

      {data && !isLoading && (
        <div className="grid grid-cols-1 lg:grid-cols-5 gap-4">
          {/* Bar chart */}
          <div className="card p-5 lg:col-span-2">
            <div className="label mb-4">Weekly slope (%/wk) — top {data.data.length}</div>
            {data.data.length === 0
              ? <p className="text-sm text-secondary py-8 text-center">No features above {slope}%/wk threshold.</p>
              : slopeBarOption && <Chart option={slopeBarOption} height={Math.max(200, data.data.length * 32 + 32)} />
            }
          </div>

          {/* Table + trend */}
          <div className="card p-5 lg:col-span-3 flex flex-col gap-4">
            <div className="label">Select a feature to inspect its trend</div>
            <div className="overflow-auto">
              <table className="w-full text-sm">
                <thead>
                  <tr className="border-b border-border">
                    {['Feature', 'Type', 'Slope', 'Avg ms', 'p95 ms'].map(h => (
                      <th key={h} className="text-left py-2 pr-4 label">{h}</th>
                    ))}
                  </tr>
                </thead>
                <tbody>
                  {data.data.map(row => (
                    <tr
                      key={row.feature}
                      onClick={() => setSelected(row)}
                      className={`border-b border-border/50 cursor-pointer table-row-hover
                        ${selected?.feature === row.feature ? 'bg-accent/5 border-l-2 border-l-accent' : ''}`}
                    >
                      <td className="py-2 pr-4 text-primary font-medium">{row.feature}</td>
                      <td className="py-2 pr-4 text-secondary text-xs">{row.feature_type}</td>
                      <td className="py-2 pr-4 font-mono text-down">+{row.weekly_slope_pct.toFixed(2)}%</td>
                      <td className="py-2 pr-4 font-mono text-primary">{row.mean_avg_ms?.toFixed(0)}</td>
                      <td className="py-2 font-mono text-warn">{row.mean_p95_ms?.toFixed(0)}</td>
                    </tr>
                  ))}
                </tbody>
              </table>
            </div>

            {selected && (
              <div>
                <div className="label mb-3">Trend: {selected.feature}</div>
                <Chart option={trendOption} height={220} />
              </div>
            )}
          </div>
        </div>
      )}
    </div>
  )
}
