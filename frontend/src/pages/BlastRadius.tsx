import { useState } from 'react'
import { useQuery } from '@tanstack/react-query'
import { api, type BlastItem } from '../api/client'
import PageHeader from '../components/PageHeader'
import Chart from '../components/Chart'
import { PageSpinner } from '../components/Spinner'
import ErrorBanner from '../components/ErrorBanner'

export default function BlastRadius() {
  const [selectedTag, setSelectedTag] = useState<string>('')
  const [window, setWindow] = useState(14)
  const [zThreshold, setZThreshold] = useState(1.0)

  const { data: deps } = useQuery({ queryKey: ['deployments'], queryFn: api.deployments })

  const { data, isLoading, error } = useQuery({
    queryKey: ['blastRadius', selectedTag, window, zThreshold],
    queryFn: () => api.blastRadius(selectedTag, window, zThreshold),
    enabled: !!selectedTag,
  })

  const tags = deps?.data.filter(c => c.tag).map(c => c.tag!) ?? []

  function deltaColor(d: BlastItem) {
    return d.delta_pct > 10 ? '#f87171' : d.delta_pct > 3 ? '#fbbf24' : d.delta_pct < -5 ? '#34d399' : '#22d3ee'
  }

  const barOption = data && {
    grid: { top: 8, right: 60, bottom: 8, left: 8, containLabel: true },
    xAxis: { type: 'value' as const, axisLabel: { formatter: (v: number) => `${v > 0 ? '+' : ''}${v.toFixed(0)}%` } },
    yAxis: { type: 'category' as const, data: [...data.data].reverse().map(d => d.feature), axisLabel: { fontSize: 11 } },
    series: [{
      type: 'bar' as const,
      data: [...data.data].reverse().map(d => ({
        value: +d.delta_pct.toFixed(2),
        itemStyle: { color: deltaColor(d) },
      })),
      barMaxWidth: 28,
    }],
    tooltip: { trigger: 'axis' as const, formatter: (p: unknown[]) => {
      const pt = (p as { name: string; value: number }[])[0]
      return `<b>${pt.name}</b><br/>Delta: ${pt.value > 0 ? '+' : ''}${pt.value.toFixed(1)}%`
    }},
  }

  return (
    <div className="space-y-6 animate-slide-up">
      <PageHeader
        tag="UC-02"
        title="Blast Radius"
        description="Before vs after latency comparison for each deployment. Z-score filtered."
        controls={
          <div className="flex items-center gap-2">
            <select className="select" value={selectedTag} onChange={e => setSelectedTag(e.target.value)}>
              <option value="">Choose a release…</option>
              {tags.map(t => <option key={t} value={t}>{t}</option>)}
            </select>
            <select className="select" value={window} onChange={e => setWindow(+e.target.value)}>
              {[7, 14, 21, 30].map(v => <option key={v} value={v}>{v}d window</option>)}
            </select>
            <select className="select" value={zThreshold} onChange={e => setZThreshold(+e.target.value)}>
              {[0.5, 1.0, 1.5, 2.0, 2.5].map(v => <option key={v} value={v}>z ≥ {v}</option>)}
            </select>
          </div>
        }
      />

      {!selectedTag && (
        <div className="card p-12 text-center text-secondary text-sm">
          Select a deployment tag to analyse its blast radius.
        </div>
      )}
      {error && <ErrorBanner error={error} />}
      {isLoading && <PageSpinner />}

      {data && !isLoading && (
        <div className="grid grid-cols-1 lg:grid-cols-5 gap-4">
          <div className="card p-5 lg:col-span-2">
            <div className="label mb-4">Delta % after {selectedTag}</div>
            {data.data.length === 0
              ? <p className="text-sm text-secondary py-8 text-center">No features crossed the z≥{zThreshold} threshold.</p>
              : barOption && <Chart option={barOption} height={Math.max(200, data.data.length * 30 + 32)} />
            }
          </div>

          <div className="card p-5 lg:col-span-3">
            <div className="label mb-4">Affected features — {selectedTag}</div>
            {data.data.length === 0
              ? <p className="text-sm text-secondary py-4">No significant changes detected.</p>
              : (
                <table className="w-full text-sm">
                  <thead>
                    <tr className="border-b border-border">
                      {['Feature', 'Before', 'After', 'Δ%', 'z-score'].map(h => (
                        <th key={h} className="text-left py-2 pr-4 label">{h}</th>
                      ))}
                    </tr>
                  </thead>
                  <tbody>
                    {data.data.map(row => (
                      <tr key={row.feature} className="border-b border-border/50 table-row-hover">
                        <td className="py-2 pr-4 text-primary font-medium">{row.feature}</td>
                        <td className="py-2 pr-4 font-mono text-secondary">{row.before_avg_ms?.toFixed(0)}</td>
                        <td className="py-2 pr-4 font-mono text-primary">{row.after_avg_ms?.toFixed(0)}</td>
                        <td className={`py-2 pr-4 font-mono ${row.delta_pct > 0 ? 'text-down' : 'text-up'}`}>
                          {row.delta_pct > 0 ? '+' : ''}{row.delta_pct?.toFixed(1)}%
                        </td>
                        <td className="py-2 font-mono text-warn">{row.z_score?.toFixed(2)}</td>
                      </tr>
                    ))}
                  </tbody>
                </table>
              )
            }
          </div>
        </div>
      )}
    </div>
  )
}
