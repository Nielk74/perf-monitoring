import { useState } from 'react'
import { useQuery } from '@tanstack/react-query'
import { api } from '../api/client'
import PageHeader from '../components/PageHeader'
import Chart from '../components/Chart'
import { PageSpinner } from '../components/Spinner'
import ErrorBanner from '../components/ErrorBanner'

export default function FrictionHeatmap() {
  const [days, setDays] = useState(30)
  const [topN, setTopN] = useState(20)

  const { data, isLoading, error } = useQuery({
    queryKey: ['frictionHeatmap', days, topN],
    queryFn: () => api.frictionHeatmap(days, topN),
  })
  const { data: hourly } = useQuery({
    queryKey: ['hourlyFriction', days],
    queryFn: () => api.hourlyFriction(days),
  })

  const barOption = data && {
    grid: { top: 8, right: 80, bottom: 8, left: 8, containLabel: true },
    xAxis: { type: 'value' as const, name: 'Hours', nameTextStyle: { color: '#737373', fontSize: 11 } },
    yAxis: { type: 'category' as const, data: [...data.data].reverse().map(d => d.feature), axisLabel: { fontSize: 11 } },
    series: [{
      type: 'bar' as const,
      data: [...data.data].reverse().map(d => ({
        value: +(d.total_wait_hours ?? 0).toFixed(2),
        itemStyle: { color: `rgba(248,113,113,${0.3 + (d.friction_score ?? 0) * 0.7})` },
      })),
      barMaxWidth: 24,
      label: { show: true, position: 'right' as const, formatter: (p: { value: number }) => `${p.value.toFixed(1)}h`, color: '#737373', fontSize: 10 },
    }],
    tooltip: { trigger: 'axis' as const, formatter: (p: unknown[]) => {
      const pt = (p as { name: string; value: number }[])[0]
      return `<b>${pt.name}</b><br/>Total wait: ${pt.value.toFixed(2)}h`
    }},
  }

  const hourlyOption = hourly && {
    grid: { top: 12, right: 8, bottom: 36, left: 52 },
    xAxis: { type: 'category' as const, data: hourly.data.map((d: { hour_utc: number }) => `${d.hour_utc}:00`) },
    yAxis: [
      { type: 'value' as const, name: 'Events', nameTextStyle: { color: '#737373', fontSize: 11 } },
      { type: 'value' as const, name: 'ms', nameTextStyle: { color: '#737373', fontSize: 11 } },
    ],
    series: [
      { name: 'Event count', type: 'bar' as const, data: hourly.data.map((d: { event_count: number }) => d.event_count), itemStyle: { color: '#6366f1', opacity: 0.6 }, barMaxWidth: 20, yAxisIndex: 0 },
      { name: 'avg ms', type: 'line' as const, data: hourly.data.map((d: { avg_ms: number }) => d.avg_ms), lineStyle: { color: '#ef4444' }, itemStyle: { color: '#ef4444' }, showSymbol: false, smooth: true, yAxisIndex: 1 },
    ],
    legend: { bottom: 0, textStyle: { color: '#737373', fontSize: 11 } },
    tooltip: { trigger: 'axis' as const },
  }

  return (
    <div className="space-y-6 animate-slide-up">
      <PageHeader
        tag="UC-06"
        title="Friction Heatmap"
        description="Total user wait time per feature. High friction = high latency × high usage volume."
        controls={
          <div className="flex items-center gap-2">
            <select className="select" value={topN} onChange={e => setTopN(+e.target.value)}>
              {[10, 20, 30].map(v => <option key={v} value={v}>Top {v}</option>)}
            </select>
            <select className="select" value={days} onChange={e => setDays(+e.target.value)}>
              {[14, 30, 60, 90].map(v => <option key={v} value={v}>Last {v}d</option>)}
            </select>
          </div>
        }
      />

      {error && <ErrorBanner error={error} />}
      {isLoading && <PageSpinner />}

      {data && !isLoading && (
        <div className="space-y-4">
          <div className="grid grid-cols-1 lg:grid-cols-5 gap-4">
            {/* Ranked bar */}
            <div className="card p-5 lg:col-span-3">
              <div className="label mb-4">Total wait time (hours)</div>
              {barOption && <Chart option={barOption} height={Math.max(220, data.data.length * 28 + 32)} />}
            </div>

            {/* Table */}
            <div className="card p-5 lg:col-span-2">
              <div className="label mb-4">Feature friction ranking</div>
              <div className="overflow-auto max-h-80">
                <table className="w-full text-sm">
                  <thead className="sticky top-0 bg-raised">
                    <tr className="border-b border-border">
                      {['#', 'Feature', 'Wait h', 'Avg ms', 'Score'].map(h => (
                        <th key={h} className="text-left py-2 pr-3 label text-xs">{h}</th>
                      ))}
                    </tr>
                  </thead>
                  <tbody>
                    {data.data.map((row, i) => (
                      <tr key={row.feature} className="border-b border-border/50 table-row-hover">
                        <td className="py-2 pr-3 text-muted font-mono text-xs">{i + 1}</td>
                        <td className="py-2 pr-3 text-primary font-medium text-xs">{row.feature}</td>
                        <td className="py-2 pr-3 font-mono text-down text-xs">{row.total_wait_hours?.toFixed(1)}</td>
                        <td className="py-2 pr-3 font-mono text-secondary text-xs">{row.avg_ms?.toFixed(0)}</td>
                        <td className="py-2 text-xs">
                          <div className="flex items-center gap-1.5">
                            <div className="h-1.5 rounded-full bg-down" style={{ width: `${(row.friction_score ?? 0) * 56}px` }} />
                            <span className="text-secondary font-mono">{((row.friction_score ?? 0) * 100).toFixed(0)}</span>
                          </div>
                        </td>
                      </tr>
                    ))}
                  </tbody>
                </table>
              </div>
            </div>
          </div>

          {/* Hourly distribution */}
          <div className="card p-5">
            <div className="label mb-4">Hourly event volume + avg latency (UTC)</div>
            {hourlyOption && <Chart option={hourlyOption} height={220} />}
          </div>
        </div>
      )}
    </div>
  )
}
