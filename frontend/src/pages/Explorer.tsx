/**
 * Explorer — cross-filter intelligence hub.
 *
 * Shared state: { days, selectedFeature, selectedFeatureType, selectedOffice }
 * Every panel reads from it AND writes to it on click.
 * Panels are individually toggleable; layout is a 3-column CSS grid.
 */
import { useState, useMemo } from 'react'
import { useQuery } from '@tanstack/react-query'
import { api, type FrictionItem, type OfficeRow, type PeakHourRow, type AnomalyItem } from '../api/client'
import Chart from '../components/Chart'
import { PageSpinner } from '../components/Spinner'

// ─── Shared state ─────────────────────────────────────────────────────────────

interface XState {
  days: number
  feature: string | null
  featureType: string | null
  office: string | null
}

// ─── Panel registry ───────────────────────────────────────────────────────────

const PANELS = [
  { id: 'features', label: 'Features',      cols: 1, defaultOn: true  },
  { id: 'trend',    label: 'Trend',          cols: 2, defaultOn: true  },
  { id: 'friction', label: 'Friction',       cols: 1, defaultOn: true  },
  { id: 'office',   label: 'Offices',        cols: 1, defaultOn: true  },
  { id: 'peak',     label: 'Peak Hours',     cols: 1, defaultOn: true  },
  { id: 'anomaly',  label: 'Anomaly Guard',  cols: 1, defaultOn: false },
  { id: 'sunset',   label: 'Sunset Risk',    cols: 1, defaultOn: false },
  { id: 'sessions', label: 'Session Pulse',  cols: 2, defaultOn: false },
] as const

type PanelId = typeof PANELS[number]['id']

const OFFICE_COLOR: Record<string, string> = { LON: '#6366f1', NYC: '#f97316', SIN: '#22c55e' }

const TYPE_COLORS = [
  '#6366f1','#22c55e','#f97316','#8b5cf6','#ec4899','#f59e0b','#3b82f6','#0ea5e9',
]

// ─── Panel container ──────────────────────────────────────────────────────────

function Panel({ title, cols, children, isEmpty }: {
  title: string
  cols: 1 | 2
  children: React.ReactNode
  isEmpty?: boolean
}) {
  const span = cols === 2 ? 'lg:col-span-2' : 'lg:col-span-1'
  return (
    <div className={`card flex flex-col min-h-0 ${span} animate-slide-up`}>
      <div className="flex items-center justify-between px-4 pt-4 pb-3 border-b border-border flex-shrink-0">
        <span className="label">{title}</span>
      </div>
      <div className="flex-1 overflow-auto p-4">
        {isEmpty
          ? <div className="flex items-center justify-center h-32 text-secondary text-sm">No data</div>
          : children
        }
      </div>
    </div>
  )
}

// ─── Filter chip ──────────────────────────────────────────────────────────────

function Chip({ label, value, onClear }: { label: string; value: string; onClear: () => void }) {
  return (
    <span className="inline-flex items-center gap-1 px-2 py-0.5 rounded-full text-xs bg-accent/10 border border-accent/20 text-accent">
      <span className="text-accent/50">{label}</span>
      {value}
      <button onClick={onClear} className="ml-0.5 hover:text-white transition-colors">
        <svg viewBox="0 0 10 10" fill="none" className="w-2.5 h-2.5" stroke="currentColor" strokeWidth="1.5">
          <line x1="2" y1="2" x2="8" y2="8"/><line x1="8" y1="2" x2="2" y2="8"/>
        </svg>
      </button>
    </span>
  )
}

// ─── Features panel ───────────────────────────────────────────────────────────

function FeaturesPanel({ xs, set }: { xs: XState; set: (p: Partial<XState>) => void }) {
  const { data } = useQuery({ queryKey: ['features'], queryFn: api.features })

  const byType = useMemo(() => {
    const map = new Map<string, typeof data>()
    data?.forEach(f => {
      if (!map.has(f.feature_type)) map.set(f.feature_type, [])
      map.get(f.feature_type)!.push(f)
    })
    return [...map.entries()]
  }, [data])

  const typeColorMap = useMemo(() => {
    const m: Record<string, string> = {}
    byType.forEach(([type], i) => { m[type] = TYPE_COLORS[i % TYPE_COLORS.length] })
    return m
  }, [byType])

  if (!data) return <PageSpinner />

  const maxCount = Math.max(...data.map(f => f.event_count), 1)

  return (
    <div className="space-y-3">
      {byType.map(([type, features]) => {
        const color = typeColorMap[type]
        const activeType = xs.featureType === type
        return (
          <div key={type}>
            <button
              onClick={() => set({ featureType: activeType ? null : type, feature: null })}
              className="w-full text-left mb-1.5"
            >
              <span
                className={`text-[10px] font-semibold tracking-wider px-1.5 py-0.5 rounded transition-all ${
                  activeType ? 'opacity-100' : 'opacity-60 hover:opacity-100'
                }`}
                style={{ color, background: `${color}18` }}
              >
                {type.toUpperCase()}
              </span>
            </button>
            <div className="space-y-0.5">
              {(features ?? [])
                .filter(f => !xs.featureType || f.feature_type === xs.featureType)
                .map(f => {
                  const active = xs.feature === f.feature
                  const barW = (f.event_count / maxCount) * 100
                  return (
                    <button
                      key={f.feature}
                      onClick={() => set({ feature: active ? null : f.feature, featureType: f.feature_type })}
                      className={`w-full flex items-center gap-2 px-2 py-1 rounded text-xs text-left group transition-all ${
                        active
                          ? 'bg-accent/10 border border-accent/20'
                          : 'hover:bg-raised border border-transparent'
                      }`}
                    >
                      <span className={`flex-1 truncate ${active ? 'text-accent' : 'text-primary'}`}>
                        {f.feature}
                      </span>
                      <span className="relative w-16 h-1 bg-border rounded-full flex-shrink-0">
                        <span
                          className="absolute inset-y-0 left-0 rounded-full"
                          style={{ width: `${barW}%`, background: color, opacity: active ? 1 : 0.5 }}
                        />
                      </span>
                      <span className="text-[10px] font-mono text-muted w-8 text-right flex-shrink-0">
                        {f.event_count >= 1000 ? `${(f.event_count/1000).toFixed(0)}k` : f.event_count}
                      </span>
                    </button>
                  )
                })}
            </div>
          </div>
        )
      })}
    </div>
  )
}

// ─── Trend panel ──────────────────────────────────────────────────────────────

function TrendPanel({ xs }: { xs: XState }) {
  const { data, isLoading } = useQuery({
    queryKey: ['xTrend', xs.feature, xs.days],
    queryFn: () => api.featureTrend(xs.feature!, xs.days),
    enabled: !!xs.feature,
  })
  const { data: fallback } = useQuery({
    queryKey: ['silentDegrader', 0.1],
    queryFn: () => api.silentDegrader(0.1, 5),
    enabled: !xs.feature,
  })

  if (!xs.feature) {
    return (
      <div className="space-y-3">
        <p className="text-xs text-secondary mb-3">Select a feature to see its latency trend. Top degraders:</p>
        {fallback?.data.slice(0, 5).map(d => (
          <div key={d.feature} className="flex items-center justify-between text-xs">
            <span className="text-primary truncate">{d.feature}</span>
            <span className="font-mono text-down ml-2 flex-shrink-0">+{d.weekly_slope_pct.toFixed(1)}%/wk</span>
          </div>
        ))}
      </div>
    )
  }

  if (isLoading) return <PageSpinner />

  const option = {
    grid: { top: 16, right: 8, bottom: 28, left: 48 },
    xAxis: { type: 'category', data: data?.data.map((d: Record<string, unknown>) => String(d.stat_date ?? '').slice(5)) ?? [] },
    yAxis: { type: 'value', name: 'ms', nameTextStyle: { color: '#737373', fontSize: 10 } },
    series: [
      { name: 'avg', type: 'line', smooth: true, showSymbol: false,
        data: data?.data.map((d: Record<string, unknown>) => d.avg_ms as number) ?? [],
        lineStyle: { color: '#6366f1', width: 2 }, areaStyle: { color: 'rgba(99,102,241,0.06)' }, itemStyle: { color: '#6366f1' } },
      { name: 'p95', type: 'line', smooth: true, showSymbol: false,
        data: data?.data.map((d: Record<string, unknown>) => d.p95_ms as number) ?? [],
        lineStyle: { color: '#ef4444', type: 'dashed', width: 1.5 }, itemStyle: { color: '#ef4444' } },
    ],
    legend: { bottom: 0, textStyle: { color: '#737373', fontSize: 10 } },
    tooltip: { trigger: 'axis' },
  }

  return (
    <div>
      <p className="text-xs text-accent mb-3 font-medium">{xs.feature}</p>
      <Chart option={option} height={240} />
    </div>
  )
}

// ─── Friction panel ───────────────────────────────────────────────────────────

function FrictionPanel({ xs, set }: { xs: XState; set: (p: Partial<XState>) => void }) {
  const { data, isLoading } = useQuery({
    queryKey: ['xFriction', xs.days],
    queryFn: () => api.frictionHeatmap(xs.days, 30),
  })

  const filtered = useMemo(() => {
    if (!data?.data) return []
    return data.data
      .filter(r => !xs.featureType || r.feature_type === xs.featureType)
      .slice(0, 15)
  }, [data, xs.featureType])

  if (isLoading) return <PageSpinner />

  const max = Math.max(...filtered.map(r => r.total_wait_hours ?? 0), 1)

  return (
    <div className="space-y-1">
      {filtered.map((r: FrictionItem) => {
        const active = xs.feature === r.feature
        const pct = ((r.total_wait_hours ?? 0) / max) * 100
        return (
          <button
            key={r.feature}
            onClick={() => set({ feature: active ? null : r.feature, featureType: r.feature_type })}
            className={`w-full flex items-center gap-2 px-2 py-1.5 rounded text-xs text-left transition-all group ${
              active ? 'bg-accent/10 border border-accent/20' : 'hover:bg-raised border border-transparent'
            }`}
          >
            <span className={`flex-1 truncate ${active ? 'text-accent' : 'text-primary'}`}>{r.feature}</span>
            <span className="relative w-20 h-1.5 bg-border rounded-full flex-shrink-0">
              <span className="absolute inset-y-0 left-0 rounded-full bg-down" style={{ width: `${pct}%`, opacity: 0.6 + (r.friction_score ?? 0) * 0.4 }} />
            </span>
            <span className="font-mono text-[10px] text-down w-10 text-right flex-shrink-0">
              {(r.total_wait_hours ?? 0).toFixed(1)}h
            </span>
          </button>
        )
      })}
    </div>
  )
}

// ─── Office panel ─────────────────────────────────────────────────────────────

function OfficePanel({ xs, set }: { xs: XState; set: (p: Partial<XState>) => void }) {
  const { data, isLoading } = useQuery({
    queryKey: ['xOffice', xs.days, xs.featureType],
    queryFn: () => api.envFingerprint(xs.days, xs.featureType ?? undefined),
  })

  const filtered = useMemo(() => {
    if (!data?.data) return []
    return xs.feature
      ? data.data.filter((r: OfficeRow) => r.feature === xs.feature)
      : data.data
  }, [data, xs.feature])

  // Aggregate by office
  const byOffice = useMemo(() => {
    const m = new Map<string, { avg_ms: number[]; p95_ms: number[] }>()
    filtered.forEach((r: OfficeRow) => {
      if (!m.has(r.office_prefix)) m.set(r.office_prefix, { avg_ms: [], p95_ms: [] })
      const e = m.get(r.office_prefix)!
      if (r.avg_ms) e.avg_ms.push(r.avg_ms)
      if (r.p95_ms) e.p95_ms.push(r.p95_ms)
    })
    return [...m.entries()].map(([office, vals]) => ({
      office,
      avg_ms: vals.avg_ms.reduce((a, b) => a + b, 0) / (vals.avg_ms.length || 1),
      p95_ms: vals.p95_ms.reduce((a, b) => a + b, 0) / (vals.p95_ms.length || 1),
    })).sort((a, b) => a.office.localeCompare(b.office))
  }, [filtered])

  if (isLoading) return <PageSpinner />
  if (!byOffice.length) return <div className="flex items-center justify-center h-32 text-secondary text-sm">No data</div>

  const option = {
    grid: { top: 8, right: 8, bottom: 28, left: 48 },
    xAxis: { type: 'category', data: byOffice.map(o => o.office) },
    yAxis: { type: 'value', name: 'ms', nameTextStyle: { color: '#737373', fontSize: 10 } },
    series: [
      { name: 'avg', type: 'bar', data: byOffice.map(o => +o.avg_ms.toFixed(1)),
        itemStyle: { color: (p: { dataIndex: number }) => OFFICE_COLOR[byOffice[p.dataIndex].office] ?? '#737373' },
        barMaxWidth: 40, barGap: '10%' },
      { name: 'p95', type: 'bar', data: byOffice.map(o => +o.p95_ms.toFixed(1)),
        itemStyle: { color: (p: { dataIndex: number }) => `${OFFICE_COLOR[byOffice[p.dataIndex].office] ?? '#737373'}60` },
        barMaxWidth: 40 },
    ],
    legend: { bottom: 0, textStyle: { color: '#737373', fontSize: 10 } },
    tooltip: { trigger: 'axis' },
  }

  return (
    <div>
      {xs.feature && <p className="text-xs text-accent mb-2 font-medium">{xs.feature}</p>}
      <Chart option={option} height={210}
        onEvents={{
          click: (p: unknown) => {
            const ev = p as Record<string, unknown>
            if (typeof ev.name === 'string') set({ office: xs.office === ev.name ? null : ev.name })
          },
        }}
      />
      <div className="flex gap-2 mt-2">
        {byOffice.map(o => (
          <button
            key={o.office}
            onClick={() => set({ office: xs.office === o.office ? null : o.office })}
            className={`flex-1 py-1 rounded text-xs font-medium transition-all border ${
              xs.office === o.office
                ? 'border-current opacity-100'
                : 'border-border opacity-50 hover:opacity-100'
            }`}
            style={{ color: OFFICE_COLOR[o.office] ?? '#737373' }}
          >
            {o.office}
          </button>
        ))}
      </div>
    </div>
  )
}

// ─── Peak Hours panel ─────────────────────────────────────────────────────────

function PeakPanel({ xs }: { xs: XState }) {
  const { data, isLoading } = useQuery({
    queryKey: ['xPeak', xs.days, xs.office],
    queryFn: () => api.peakHours(xs.days, xs.office ?? undefined),
  })

  if (isLoading) return <PageSpinner />
  if (!data?.data.length) return <div className="flex items-center justify-center h-32 text-secondary text-sm">No data</div>

  const DAYS = ['Sun', 'Mon', 'Tue', 'Wed', 'Thu', 'Fri', 'Sat']
  const heatData = (data.data as PeakHourRow[]).map(d => [d.hour_utc, d.day_of_week, d.event_count])
  const maxC = Math.max(...(data.data as PeakHourRow[]).map(d => d.event_count), 1)

  const option = {
    grid: { top: 8, right: 8, bottom: 40, left: 36 },
    xAxis: { type: 'category', data: Array.from({ length: 24 }, (_, i) => `${i}`), axisLabel: { fontSize: 9, interval: 2 } },
    yAxis: { type: 'category', data: DAYS, axisLabel: { fontSize: 9 } },
    visualMap: {
      min: 0, max: maxC,
      show: false,
      inRange: { color: ['#f5f3ff', '#a5b4fc', '#4f46e5'] },
    },
    series: [{ type: 'heatmap', data: heatData, emphasis: { itemStyle: { shadowBlur: 6, shadowColor: 'rgba(99,102,241,0.3)' } } }],
    tooltip: { trigger: 'item', formatter: (p: Record<string, unknown>) => {
      const v = p.value as [number, number, number]
      return `<b>${DAYS[v[1]]} ${v[0]}:00</b><br/>${v[2]} events`
    }},
  }

  return <Chart option={option} height={220} />
}

// ─── Anomaly panel ────────────────────────────────────────────────────────────

function AnomalyPanel({ xs, set }: { xs: XState; set: (p: Partial<XState>) => void }) {
  const { data, isLoading } = useQuery({
    queryKey: ['xAnomaly', 2.0, 3],
    queryFn: () => api.anomalies(2.0, 3),
  })

  const filtered = useMemo(() => {
    if (!data?.data) return []
    return data.data.filter(r => !xs.featureType || r.feature_type === xs.featureType)
  }, [data, xs.featureType])

  if (isLoading) return <PageSpinner />
  if (!filtered.length) return (
    <div className="flex flex-col items-center justify-center h-32 gap-2">
      <svg viewBox="0 0 16 16" fill="none" className="w-5 h-5 text-up" stroke="currentColor" strokeWidth="1.5">
        <path d="M2 8 L6.5 12.5 L14 4"/><circle cx="8" cy="8" r="7"/>
      </svg>
      <span className="text-xs text-secondary">All clear — z &lt; 2.0</span>
    </div>
  )

  return (
    <div className="space-y-1">
      {filtered.map((r: AnomalyItem) => {
        const active = xs.feature === r.feature
        return (
          <button
            key={r.feature}
            onClick={() => set({ feature: active ? null : r.feature, featureType: r.feature_type })}
            className={`w-full flex items-center gap-2 px-2 py-1.5 rounded text-xs text-left transition-all ${
              active ? 'bg-accent/10 border border-accent/20' : 'hover:bg-raised border border-transparent'
            }`}
          >
            <span className={`flex-1 truncate ${active ? 'text-accent' : 'text-primary'}`}>{r.feature}</span>
            <span className={`font-mono flex-shrink-0 ${r.z_score > 0 ? 'text-down' : 'text-up'}`}>
              {r.z_score > 0 ? '+' : ''}{r.z_score.toFixed(2)}z
            </span>
            <span className={`font-mono flex-shrink-0 ${r.delta_pct > 0 ? 'text-down' : 'text-up'}`}>
              {r.delta_pct > 0 ? '+' : ''}{r.delta_pct.toFixed(0)}%
            </span>
          </button>
        )
      })}
    </div>
  )
}

// ─── Sunset risk panel ────────────────────────────────────────────────────────

function SunsetPanel({ xs, set }: { xs: XState; set: (p: Partial<XState>) => void }) {
  const { data, isLoading } = useQuery({
    queryKey: ['xSunset', xs.days],
    queryFn: () => api.featureSunset(xs.days, 20),
  })

  const filtered = useMemo(() => {
    if (!data?.data) return []
    return data.data.filter(r => !xs.featureType || r.feature_type === xs.featureType)
  }, [data, xs.featureType])

  if (isLoading) return <PageSpinner />
  if (!filtered.length) return <div className="flex items-center justify-center h-32 text-secondary text-sm">No declining features</div>

  return (
    <div className="space-y-1">
      {filtered.map(r => {
        const active = xs.feature === r.feature
        return (
          <button
            key={r.feature}
            onClick={() => set({ feature: active ? null : r.feature, featureType: r.feature_type })}
            className={`w-full flex items-center gap-2 px-2 py-1.5 rounded text-xs text-left transition-all ${
              active ? 'bg-accent/10 border border-accent/20' : 'hover:bg-raised border border-transparent'
            }`}
          >
            <span className={`flex-1 truncate ${active ? 'text-accent' : 'text-primary'}`}>{r.feature}</span>
            <span className="relative w-16 h-1 bg-border rounded-full flex-shrink-0">
              <span className="absolute inset-y-0 left-0 rounded-full bg-down opacity-60" style={{ width: `${Math.min(r.decline_pct, 100)}%` }} />
            </span>
            <span className="font-mono text-down flex-shrink-0">{r.decline_pct.toFixed(0)}%</span>
          </button>
        )
      })}
    </div>
  )
}

// ─── Session pulse panel ──────────────────────────────────────────────────────

function SessionPanel({ xs }: { xs: XState }) {
  const { data, isLoading } = useQuery({
    queryKey: ['xTimeline', xs.days],
    queryFn: () => api.impersonationTimeline(xs.days),
  })
  const { data: daily, isLoading: dayL } = useQuery({
    queryKey: ['dailyPressure', xs.days],
    queryFn: () => api.dailyPressure(xs.days),
  })

  if (isLoading || dayL) return <PageSpinner />

  const option = {
    grid: { top: 12, right: 8, bottom: 28, left: 48 },
    xAxis: { type: 'category', data: daily?.data.map(d => String(d.stat_date).slice(5)) ?? [], axisLabel: { fontSize: 9, interval: 5 } },
    yAxis: [
      { type: 'value', name: 'Events', nameTextStyle: { color: '#737373', fontSize: 10 } },
      { type: 'value', name: 'Sessions', nameTextStyle: { color: '#737373', fontSize: 10 } },
    ],
    series: [
      { name: 'Events', type: 'bar', data: daily?.data.map(d => d.total_events) ?? [],
        itemStyle: { color: '#6366f1', opacity: 0.4 }, barMaxWidth: 12, yAxisIndex: 0 },
      { name: 'Impersonation', type: 'line', smooth: true, showSymbol: false,
        data: data?.data.map((d: Record<string, unknown>) => d.session_count) ?? [],
        lineStyle: { color: '#6366f1', width: 1.5 }, itemStyle: { color: '#6366f1' }, yAxisIndex: 1 },
    ],
    legend: { bottom: 0, textStyle: { color: '#737373', fontSize: 10 } },
    tooltip: { trigger: 'axis' },
  }

  return <Chart option={option} height={220} />
}

// ─── Main Explorer ────────────────────────────────────────────────────────────

export default function Explorer() {
  const [xs, setXs] = useState<XState>({ days: 30, feature: null, featureType: null, office: null })
  const [visible, setVisible] = useState<Set<PanelId>>(
    new Set(PANELS.filter(p => p.defaultOn).map(p => p.id))
  )

  const set = (patch: Partial<XState>) => setXs(prev => ({ ...prev, ...patch }))

  const togglePanel = (id: PanelId) =>
    setVisible(prev => {
      const next = new Set(prev)
      next.has(id) ? next.delete(id) : next.add(id)
      return next
    })

  const hasFilters = xs.feature || xs.featureType || xs.office

  return (
    <div className="flex flex-col gap-4 h-full">
      {/* ── Toolbar ── */}
      <div className="flex items-start gap-4 flex-wrap">
        {/* Panel toggles */}
        <div className="flex items-center gap-1 flex-wrap">
          {PANELS.map(p => {
            const on = visible.has(p.id)
            return (
              <button
                key={p.id}
                onClick={() => togglePanel(p.id)}
                className={`px-2.5 py-1 rounded text-xs font-medium border transition-all ${
                  on
                    ? 'bg-accent/10 border-accent/30 text-accent'
                    : 'bg-transparent border-border text-secondary hover:text-primary hover:border-muted'
                }`}
              >
                {on && <span className="mr-1 opacity-60">●</span>}{p.label}
              </button>
            )
          })}
        </div>

        {/* Date range */}
        <div className="flex items-center gap-1 ml-auto">
          {[14, 30, 60, 90].map(d => (
            <button
              key={d}
              onClick={() => set({ days: d })}
              className={`px-2 py-1 rounded text-xs font-mono transition-all border ${
                xs.days === d
                  ? 'bg-raised border-border text-primary'
                  : 'border-transparent text-muted hover:text-secondary'
              }`}
            >
              {d}d
            </button>
          ))}
        </div>
      </div>

      {/* ── Active filter chips ── */}
      {hasFilters && (
        <div className="flex items-center gap-2 flex-wrap -mt-1">
          {xs.featureType && <Chip label="type:" value={xs.featureType} onClear={() => set({ featureType: null })} />}
          {xs.feature    && <Chip label="feature:" value={xs.feature} onClear={() => set({ feature: null })} />}
          {xs.office     && <Chip label="office:" value={xs.office} onClear={() => set({ office: null })} />}
          <button
            onClick={() => setXs(prev => ({ ...prev, feature: null, featureType: null, office: null }))}
            className="text-xs text-muted hover:text-secondary transition-colors ml-1"
          >
            Clear all
          </button>
        </div>
      )}

      {/* ── Panel grid ── */}
      <div className="grid grid-cols-1 lg:grid-cols-3 gap-4 items-start flex-1 min-h-0 pb-2">

        {visible.has('features') && (
          <Panel title="Features" cols={1}>
            <FeaturesPanel xs={xs} set={set} />
          </Panel>
        )}

        {visible.has('trend') && (
          <Panel title="Latency Trend" cols={2}>
            <TrendPanel xs={xs} />
          </Panel>
        )}

        {visible.has('friction') && (
          <Panel title="Friction Ranking" cols={1}>
            <FrictionPanel xs={xs} set={set} />
          </Panel>
        )}

        {visible.has('office') && (
          <Panel title="Office Comparison" cols={1}>
            <OfficePanel xs={xs} set={set} />
          </Panel>
        )}

        {visible.has('peak') && (
          <Panel title={`Peak Hours${xs.office ? ` — ${xs.office}` : ''}`} cols={1}>
            <PeakPanel xs={xs} />
          </Panel>
        )}

        {visible.has('anomaly') && (
          <Panel title="Anomaly Guard" cols={1}>
            <AnomalyPanel xs={xs} set={set} />
          </Panel>
        )}

        {visible.has('sunset') && (
          <Panel title="Sunset Risk" cols={1}>
            <SunsetPanel xs={xs} set={set} />
          </Panel>
        )}

        {visible.has('sessions') && (
          <Panel title="Session Pulse — events vs impersonation" cols={2}>
            <SessionPanel xs={xs} />
          </Panel>
        )}

      </div>
    </div>
  )
}
