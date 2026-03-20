import { useState } from 'react'
import { useQuery } from '@tanstack/react-query'
import { api } from '../api/client'
import PageHeader from '../components/PageHeader'
import Chart from '../components/Chart'
import { PageSpinner } from '../components/Spinner'
import ErrorBanner from '../components/ErrorBanner'

const DAYS = ['Sun', 'Mon', 'Tue', 'Wed', 'Thu', 'Fri', 'Sat']

export default function PeakPressure() {
  const [days, setDays] = useState(30)
  const [office, setOffice] = useState('')

  const { data: offices } = useQuery({ queryKey: ['offices'], queryFn: api.offices })

  const { data, isLoading, error } = useQuery({
    queryKey: ['peakHours', days, office],
    queryFn: () => api.peakHours(days, office || undefined),
  })
  const { data: daily } = useQuery({
    queryKey: ['dailyPressure', 90],
    queryFn: () => api.dailyPressure(90),
  })
  const { data: overlap } = useQuery({
    queryKey: ['officeOverlap', days],
    queryFn: () => api.officePeakOverlap(days),
  })

  // Build heatmap: rows = days, cols = hours
  const heatData: [number, number, number][] = []
  const maxCount = data ? Math.max(...data.data.map(d => d.event_count), 1) : 1
  data?.data.forEach(d => {
    heatData.push([d.hour_utc, d.day_of_week, d.event_count])
  })

  const heatOption = {
    grid: { top: 16, right: 24, bottom: 40, left: 40 },
    xAxis: { type: 'category' as const, data: Array.from({ length: 24 }, (_, i) => `${i}:00`), splitArea: { show: true } },
    yAxis: { type: 'category' as const, data: DAYS, splitArea: { show: true } },
    visualMap: {
      min: 0, max: maxCount,
      calculable: true,
      orient: 'horizontal' as const,
      bottom: 0,
      left: 'center',
      inRange: { color: ['#f5f3ff', '#a5b4fc', '#4f46e5'] },
      textStyle: { color: '#737373', fontSize: 10 },
    },
    series: [{
      type: 'heatmap' as const,
      data: heatData,
      label: { show: false },
      emphasis: { itemStyle: { shadowBlur: 8, shadowColor: 'rgba(99,102,241,0.3)' } },
    }],
    tooltip: { trigger: 'item' as const, formatter: (p: { value: [number, number, number] }) =>
      `<b>${DAYS[p.value[1]]} ${p.value[0]}:00</b><br/>Events: ${p.value[2]}`
    },
  }

  const dailyOption = daily && {
    grid: { top: 12, right: 8, bottom: 36, left: 52 },
    xAxis: { type: 'category' as const, data: daily.data.map(d => String(d.stat_date).slice(5)), axisLabel: { interval: 6 } },
    yAxis: [
      { type: 'value' as const, name: 'Events', nameTextStyle: { color: '#737373', fontSize: 11 } },
      { type: 'value' as const, name: 'ms', nameTextStyle: { color: '#737373', fontSize: 11 } },
    ],
    series: [
      { name: 'Events', type: 'bar' as const, data: daily.data.map(d => d.total_events), itemStyle: { color: '#6366f1', opacity: 0.5 }, barMaxWidth: 16, yAxisIndex: 0 },
      { name: 'p95 ms', type: 'line' as const, data: daily.data.map(d => d.avg_p95_ms), lineStyle: { color: '#f59e0b' }, itemStyle: { color: '#f59e0b' }, showSymbol: false, smooth: true, yAxisIndex: 1 },
    ],
    legend: { bottom: 0, textStyle: { color: '#737373', fontSize: 11 } },
    tooltip: { trigger: 'axis' as const },
  }

  // Office overlap stacked
  const overlapRows = (overlap?.data as Record<string, unknown>[] | undefined) ?? []
  const overlapOffices = [...new Set(overlapRows.map(r => String(r.office_prefix)))].sort()
  const OFFICE_COLORS: Record<string, string> = { LON: '#6366f1', NYC: '#f97316', SIN: '#22c55e' }
  const overlapOption = overlapRows.length > 0 && {
    grid: { top: 8, right: 8, bottom: 40, left: 48 },
    xAxis: { type: 'category' as const, data: Array.from({ length: 24 }, (_, i) => `${i}:00`) },
    yAxis: { type: 'value' as const, name: 'Events', nameTextStyle: { color: '#737373', fontSize: 11 } },
    series: overlapOffices.map(o => ({
      name: o,
      type: 'bar' as const,
      stack: 'office',
      data: Array.from({ length: 24 }, (_, h) => {
        const row = overlapRows.find(r => Number(r.hour_utc) === h && r.office_prefix === o)
        return row ? Number(row.event_count) : 0
      }),
      itemStyle: { color: OFFICE_COLORS[o] ?? '#737373' },
    })),
    legend: { bottom: 0, textStyle: { color: '#737373', fontSize: 11 } },
    tooltip: { trigger: 'axis' as const },
  }

  return (
    <div className="space-y-6 animate-slide-up">
      <PageHeader
        tag="UC-09"
        title="Peak Pressure"
        description="When is the system under maximum load? Hour × day-of-week heatmap + daily event volume."
        controls={
          <div className="flex items-center gap-2">
            <select className="select" value={office} onChange={e => setOffice(e.target.value)}>
              <option value="">All offices</option>
              {offices?.map(o => <option key={o} value={o}>{o}</option>)}
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
          {/* Heatmap */}
          <div className="card p-5">
            <div className="label mb-4">Event volume by hour × day {office && `— ${office}`}</div>
            <Chart option={heatOption} height={280} />
          </div>

          <div className="grid grid-cols-1 lg:grid-cols-2 gap-4">
            {/* Daily */}
            <div className="card p-5">
              <div className="label mb-4">Daily events + p95 latency (90d)</div>
              {dailyOption && <Chart option={dailyOption} height={220} />}
            </div>
            {/* Office overlap */}
            <div className="card p-5">
              <div className="label mb-4">Events by office per hour (UTC)</div>
              {overlapOption && <Chart option={overlapOption} height={220} />}
            </div>
          </div>
        </div>
      )}
    </div>
  )
}
