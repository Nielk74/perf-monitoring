import { useState } from 'react'
import { useQuery } from '@tanstack/react-query'
import { api } from '../api/client'
import PageHeader from '../components/PageHeader'
import Chart from '../components/Chart'
import { PageSpinner } from '../components/Spinner'
import ErrorBanner from '../components/ErrorBanner'

export default function AdoptionVelocity() {
  const [days, setDays] = useState(180)
  const { data: featuresMeta } = useQuery({ queryKey: ['features'], queryFn: api.features })
  const [selectedFeature, setSelectedFeature] = useState('Open Blotter')

  const { data, isLoading, error } = useQuery({
    queryKey: ['adoptionCurve', selectedFeature, days],
    queryFn: () => api.adoptionCurve(selectedFeature, days),
    enabled: !!selectedFeature,
  })

  const { data: newFeats } = useQuery({
    queryKey: ['newFeatures', 120],
    queryFn: () => api.newFeatures(120),
  })

  const areaOption = data && {
    grid: { top: 12, right: 8, bottom: 36, left: 52 },
    xAxis: { type: 'category' as const, data: data.data.map(d => String(d.week_start).slice(0, 10)) },
    yAxis: [
      { type: 'value' as const, name: 'Users', nameTextStyle: { color: '#737373', fontSize: 11 } },
      { type: 'value' as const, name: 'Events', nameTextStyle: { color: '#737373', fontSize: 11 } },
    ],
    series: [
      {
        name: 'Weekly users',
        type: 'line' as const, smooth: true, showSymbol: false,
        data: data.data.map(d => d.weekly_users),
        lineStyle: { color: '#6366f1', width: 2 },
        areaStyle: { color: 'rgba(99,102,241,0.08)' },
        itemStyle: { color: '#6366f1' },
        yAxisIndex: 0,
      },
      {
        name: 'Events',
        type: 'bar' as const,
        data: data.data.map(d => d.event_count),
        itemStyle: { color: '#6366f1', opacity: 0.4 },
        barMaxWidth: 20,
        yAxisIndex: 1,
      },
    ],
    legend: { bottom: 0, textStyle: { color: '#737373', fontSize: 11 } },
    tooltip: { trigger: 'axis' as const },
  }

  const features = [...new Set(featuresMeta?.map(f => f.feature) ?? [])]

  return (
    <div className="space-y-6 animate-slide-up">
      <PageHeader
        tag="UC-08"
        title="Adoption Velocity"
        description="How quickly new features spread through the user base. Weekly active user S-curves."
        controls={
          <div className="flex items-center gap-2">
            <select className="select w-48" value={selectedFeature} onChange={e => setSelectedFeature(e.target.value)}>
              {features.map(f => <option key={f} value={f}>{f}</option>)}
            </select>
            <select className="select" value={days} onChange={e => setDays(+e.target.value)}>
              {[30, 60, 90, 180, 365].map(v => <option key={v} value={v}>Last {v}d</option>)}
            </select>
          </div>
        }
      />

      {error && <ErrorBanner error={error} />}
      {isLoading && <PageSpinner />}

      {data && !isLoading && (
        <div className="space-y-4">
          <div className="card p-5">
            <div className="label mb-4">Weekly active users — {selectedFeature}</div>
            {data.data.length === 0
              ? <p className="text-sm text-secondary py-8 text-center">No data for this feature in the selected window.</p>
              : areaOption && <Chart option={areaOption} height={280} />
            }
          </div>

          {/* Stats row */}
          {data.data.length > 0 && (
            <div className="grid grid-cols-4 gap-4">
              {[
                { label: 'Peak weekly users', value: Math.max(...data.data.map(d => d.weekly_users)) },
                { label: 'Total events', value: data.data.reduce((s, d) => s + d.event_count, 0).toLocaleString() },
                { label: 'Weeks tracked', value: data.data.length },
                { label: 'Latest users/wk', value: data.data.at(-1)?.weekly_users ?? 0 },
              ].map(kpi => (
                <div key={kpi.label} className="card p-4">
                  <div className="label mb-1">{kpi.label}</div>
                  <div className="text-xl font-semibold text-primary">{kpi.value}</div>
                </div>
              ))}
            </div>
          )}

          {/* New features */}
          <div className="card p-5">
            <div className="label mb-4">New features (last 120 days)</div>
            {!newFeats?.data.length
              ? <p className="text-sm text-secondary">No new features found.</p>
              : (
                <table className="w-full text-sm">
                  <thead>
                    <tr className="border-b border-border">
                      {['Feature', 'Type', 'First seen', 'Age', 'Total events', 'Peak users/day'].map(h => (
                        <th key={h} className="text-left py-2 pr-4 label text-xs">{h}</th>
                      ))}
                    </tr>
                  </thead>
                  <tbody>
                    {(newFeats.data as Record<string, unknown>[]).map((row, i) => (
                      <tr
                        key={i}
                        className={`border-b border-border/50 cursor-pointer table-row-hover ${selectedFeature === row.feature ? 'bg-accent/5' : ''}`}
                        onClick={() => setSelectedFeature(String(row.feature))}
                      >
                        <td className="py-2 pr-4 text-primary font-medium">{String(row.feature)}</td>
                        <td className="py-2 pr-4 text-secondary text-xs">{String(row.feature_type)}</td>
                        <td className="py-2 pr-4 text-secondary text-xs">{String(row.first_seen).slice(0, 10)}</td>
                        <td className="py-2 pr-4 font-mono text-accent text-xs">{String(row.age_days)}d</td>
                        <td className="py-2 pr-4 font-mono text-primary">{Number(row.total_events).toLocaleString()}</td>
                        <td className="py-2 font-mono text-up">{String(row.peak_daily_users)}</td>
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
