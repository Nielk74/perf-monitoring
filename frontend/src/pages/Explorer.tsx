/**
 * Explorer — BI-style exploration hub.
 * Multiple charts · Line = time-series · Aggregation · Regression overlay · Fullscreen · PNG export
 */
import { useState, useMemo, useRef, useEffect, useCallback } from 'react'
import { createPortal } from 'react-dom'
import { useQuery, useQueries } from '@tanstack/react-query'
import ReactECharts from 'echarts-for-react'
import { api, type FrictionItem, type OfficeRow, type PeakHourRow, type TrendDay } from '../api/client'
import Chart from '../components/Chart'
import { PageSpinner } from '../components/Spinner'

// ── Constants ──────────────────────────────────────────────────────────────────

const DIMENSIONS = [
  { id: 'feature_type', label: 'Feature Type' },
  { id: 'feature',      label: 'Feature'      },
  { id: 'office',       label: 'Office'        },
  { id: 'hour',         label: 'Hour of Day'   },
  { id: 'day_hour',     label: 'Day × Hour'    },
] as const
type DimId = typeof DIMENSIONS[number]['id']

const METRICS = [
  { id: 'avg_ms',           label: 'Avg Latency (ms)',  short: 'Avg ms'   },
  { id: 'p95_ms',           label: 'P95 Latency (ms)',  short: 'P95 ms'   },
  { id: 'event_count',      label: 'Event Count',       short: 'Events'   },
  { id: 'friction_score',   label: 'Friction Score',    short: 'Friction' },
  { id: 'total_wait_hours', label: 'Total Wait (hrs)',  short: 'Wait hrs' },
] as const
type MetricId = typeof METRICS[number]['id']

const CHART_TYPES = [
  { id: 'bar',     label: 'Bar'     },
  { id: 'line',    label: 'Line'    },
  { id: 'scatter', label: 'Scatter' },
  { id: 'heatmap', label: 'Heatmap' },
] as const
type ChartTypeId = typeof CHART_TYPES[number]['id']

const AGGREGATIONS = [
  { id: 'avg',    label: 'Avg'    },
  { id: 'sum',    label: 'Sum'    },
  { id: 'max',    label: 'Max'    },
  { id: 'min',    label: 'Min'    },
  { id: 'median', label: 'Median' },
] as const
type AggId = typeof AGGREGATIONS[number]['id']

const DAYS_OPTIONS   = [7, 14, 30, 60, 90, 180, 365]
const PALETTE        = ['#6366f1','#22c55e','#f97316','#8b5cf6','#ec4899','#f59e0b','#3b82f6','#0ea5e9','#14b8a6','#f43f5e']
const OFFICE_COLOR: Record<string, string> = { LON: '#6366f1', NYC: '#f97316', SIN: '#22c55e' }
const TIME_SERIES_METRICS = new Set<MetricId>(['avg_ms', 'p95_ms', 'event_count'])

// ── Types ──────────────────────────────────────────────────────────────────────

interface TabFilter {
  featureTypes: string[]
  features:     string[]
  offices:      string[]
  days:         number
}

interface ChartConfig {
  id:             string
  dimension:      DimId
  metric:         MetricId
  chartType:      ChartTypeId
  aggregation:    AggId
  showRegression: boolean
}

interface TabState {
  id:     string
  name:   string
  filter: TabFilter
  charts: ChartConfig[]
}

interface Bookmark {
  id: string; name: string; tabs: TabState[]; savedAt: string
}

// ── Utils ──────────────────────────────────────────────────────────────────────

function uid() { return Math.random().toString(36).slice(2, 9) }

function defaultChart(): ChartConfig {
  return { id: uid(), dimension: 'feature_type', metric: 'avg_ms', chartType: 'bar', aggregation: 'avg', showRegression: false }
}
function defaultTab(name = 'Tab 1'): TabState {
  return { id: uid(), name, filter: { featureTypes: [], features: [], offices: [], days: 30 }, charts: [defaultChart()] }
}

function bmsLoad(): Bookmark[] {
  try { return JSON.parse(localStorage.getItem('explorer-bookmarks') ?? '[]') } catch { return [] }
}
function bmsSave(bms: Bookmark[]) { localStorage.setItem('explorer-bookmarks', JSON.stringify(bms)) }

function sessionLoad(): { tabs: TabState[]; activeId: string } | null {
  try { const r = localStorage.getItem('explorer-session'); return r ? JSON.parse(r) : null } catch { return null }
}
function sessionSave(tabs: TabState[], activeId: string) {
  try { localStorage.setItem('explorer-session', JSON.stringify({ tabs, activeId })) } catch { /* quota */ }
}

function downloadJson(data: unknown, filename: string) {
  const blob = new Blob([JSON.stringify(data, null, 2)], { type: 'application/json' })
  const url = URL.createObjectURL(blob)
  Object.assign(document.createElement('a'), { href: url, download: filename }).click()
  URL.revokeObjectURL(url)
}

function frictionVal(r: FrictionItem, metric: MetricId): number {
  if (metric === 'event_count') return r.total_events ?? 0
  return (r[metric as keyof FrictionItem] as number) ?? 0
}

function trendVal(row: TrendDay, metric: MetricId): number | null {
  if (metric === 'avg_ms')      return row.avg_ms
  if (metric === 'p95_ms')      return row.p95_ms
  if (metric === 'event_count') return row.event_count
  return null
}

function applyAgg(vals: number[], agg: AggId): number {
  if (vals.length === 0) return 0
  switch (agg) {
    case 'sum': return vals.reduce((a, b) => a + b, 0)
    case 'max': return Math.max(...vals)
    case 'min': return Math.min(...vals)
    case 'median': {
      const s = [...vals].sort((a, b) => a - b)
      const m = Math.floor(s.length / 2)
      return s.length % 2 === 0 ? (s[m - 1] + s[m]) / 2 : s[m]
    }
    default: return vals.reduce((a, b) => a + b, 0) / vals.length
  }
}

function linReg(data: (number | null)[]): (number | null)[] {
  const pts = data.map((y, x) => y !== null ? { x, y } : null).filter(Boolean) as { x: number; y: number }[]
  if (pts.length < 2) return data.map(() => null)
  const n = pts.length, sx = pts.reduce((s, p) => s + p.x, 0), sy = pts.reduce((s, p) => s + p.y, 0)
  const sxy = pts.reduce((s, p) => s + p.x * p.y, 0), sx2 = pts.reduce((s, p) => s + p.x * p.x, 0)
  const slope = (n * sxy - sx * sy) / (n * sx2 - sx * sx)
  const intercept = (sy - slope * sx) / n
  return data.map((_, x) => +(slope * x + intercept).toFixed(2))
}

function fmtVal(v: number | null): string {
  if (v === null || v === undefined) return '—'
  return Number.isInteger(v) ? v.toLocaleString() : v.toLocaleString(undefined, { maximumFractionDigits: 1 })
}

// ── MultiSelect with search ───────────────────────────────────────────────────

function MultiSelect({ label, options, selected, onChange }: {
  label: string; options: string[]; selected: string[]; onChange: (v: string[]) => void
}) {
  const [open, setOpen]     = useState(false)
  const [search, setSearch] = useState('')
  const ref = useRef<HTMLDivElement>(null)

  useEffect(() => {
    const h = (e: MouseEvent) => { if (ref.current && !ref.current.contains(e.target as Node)) setOpen(false) }
    document.addEventListener('mousedown', h)
    return () => document.removeEventListener('mousedown', h)
  }, [])

  const filtered = useMemo(() =>
    search ? options.filter(o => o.toLowerCase().includes(search.toLowerCase())) : options,
    [options, search])

  const toggle = (v: string) => onChange(selected.includes(v) ? selected.filter(x => x !== v) : [...selected, v])

  return (
    <div className="relative" ref={ref}>
      {/* Trigger: label + selected chips */}
      <button
        onClick={() => { setOpen(p => !p); setSearch('') }}
        className={`flex items-center gap-1.5 px-2.5 py-1 rounded text-xs border transition-all ${selected.length > 0 ? 'border-accent/40 bg-accent/5' : 'border-border text-secondary hover:text-primary'}`}
      >
        <span className={selected.length > 0 ? 'text-secondary' : ''}>{label}</span>
        {selected.length > 0 && selected.map(s => (
          <span key={s} className="flex items-center gap-0.5 px-1.5 py-px rounded-sm text-[10px] font-medium bg-[#52b154] text-white leading-none">
            {s}
            <svg width="8" height="8" viewBox="0 0 10 10" fill="currentColor" className="opacity-80 flex-shrink-0"
              onClick={e => { e.stopPropagation(); onChange(selected.filter(x => x !== s)) }}>
              <path d="M1 1l8 8M9 1l-8 8" stroke="white" strokeWidth="1.8" fill="none"/>
            </svg>
          </span>
        ))}
        <svg viewBox="0 0 8 5" className={`w-2 h-2 flex-shrink-0 transition-transform text-secondary ${open ? 'rotate-180' : ''}`} fill="currentColor"><path d="M0 0 L4 5 L8 0z"/></svg>
      </button>

      {open && (
        <div className="absolute top-full left-0 mt-1 z-50 bg-white border border-[#d9d9d9] shadow-lg min-w-[220px] overflow-hidden" style={{ fontFamily: 'inherit' }}>
          {/* Header with search + action icons */}
          <div className="flex items-center gap-1 px-2 py-1.5 border-b border-[#e0e0e0] bg-[#f5f5f5]">
            {/* Search */}
            <div className="flex items-center flex-1 gap-1 bg-white border border-[#d0d0d0] rounded px-1.5 py-0.5">
              <svg width="11" height="11" viewBox="0 0 16 16" fill="none" stroke="#999" strokeWidth="2"><circle cx="6.5" cy="6.5" r="4.5"/><path d="M10.5 10.5l3 3"/></svg>
              <input
                autoFocus
                value={search}
                onChange={e => setSearch(e.target.value)}
                placeholder="Search…"
                className="w-full text-[11px] text-[#333] placeholder:text-[#aaa] outline-none bg-transparent"
              />
            </div>
            {/* Select all */}
            <button onClick={() => onChange(options)} title="Select all" className="p-0.5 rounded hover:bg-[#e0e0e0] text-[#555] hover:text-[#222]">
              <svg width="14" height="14" viewBox="0 0 16 16" fill="none" stroke="currentColor" strokeWidth="1.8"><rect x="1.5" y="1.5" width="13" height="13" rx="1.5"/><path d="M4 8l2.5 2.5L12 5"/></svg>
            </button>
            {/* Select none */}
            <button onClick={() => onChange([])} title="Select none" className="p-0.5 rounded hover:bg-[#e0e0e0] text-[#555] hover:text-[#222]">
              <svg width="14" height="14" viewBox="0 0 16 16" fill="none" stroke="currentColor" strokeWidth="1.8"><rect x="1.5" y="1.5" width="13" height="13" rx="1.5"/><path d="M5 5l6 6M11 5l-6 6"/></svg>
            </button>
            {/* Invert */}
            <button onClick={() => onChange(options.filter(o => !selected.includes(o)))} title="Invert selection" className="p-0.5 rounded hover:bg-[#e0e0e0] text-[#555] hover:text-[#222]">
              <svg width="14" height="14" viewBox="0 0 16 16" fill="none" stroke="currentColor" strokeWidth="1.8"><path d="M2 5h10M9 2l3 3-3 3"/><path d="M14 11H4M7 8l-3 3 3 3"/></svg>
            </button>
          </div>

          {/* Options list */}
          <div className="max-h-52 overflow-y-auto">
            {filtered.length === 0
              ? <div className="px-3 py-2 text-[11px] text-[#999]">No match</div>
              : filtered.map(opt => {
                  const isSel = selected.includes(opt)
                  return (
                    <div
                      key={opt}
                      onClick={() => toggle(opt)}
                      className={`flex items-center justify-between px-3 py-[5px] cursor-pointer select-none text-[12px] ${isSel ? 'bg-[#52b154] text-white' : 'text-[#333] hover:bg-[#f0f0f0]'}`}
                    >
                      <span className="truncate">{opt}</span>
                      {isSel && (
                        <svg width="12" height="12" viewBox="0 0 12 12" fill="none" stroke="white" strokeWidth="2" className="flex-shrink-0 ml-2"><path d="M2 6l3 3 5-5"/></svg>
                      )}
                    </div>
                  )
                })
            }
          </div>
        </div>
      )}
    </div>
  )
}

// ── Filter chip ────────────────────────────────────────────────────────────────

function Chip({ label, value, onClear }: { label: string; value: string; onClear: () => void }) {
  return (
    <span className="inline-flex items-center gap-1 px-2 py-0.5 rounded-full text-xs bg-accent/10 border border-accent/20 text-accent">
      <span className="text-accent/50">{label}</span>{value}
      <button onClick={onClear} className="ml-0.5 hover:text-white transition-colors">
        <svg viewBox="0 0 10 10" fill="none" className="w-2.5 h-2.5" stroke="currentColor" strokeWidth="1.5">
          <line x1="2" y1="2" x2="8" y2="8"/><line x1="8" y1="2" x2="2" y2="8"/>
        </svg>
      </button>
    </span>
  )
}

// ── Bookmark panel ─────────────────────────────────────────────────────────────

function BookmarkPanel({ bookmarks, currentTabs, onSave, onLoad, onDelete, onExport, onImportClick }: {
  bookmarks: Bookmark[]; currentTabs: TabState[]
  onSave: (name: string) => void; onLoad: (bm: Bookmark) => void
  onDelete: (id: string) => void; onExport: (bm: Bookmark) => void; onImportClick: () => void
}) {
  const [newName, setNewName] = useState('')
  return (
    <div className="card border-accent/20 p-4 flex-shrink-0">
      <div className="flex items-center justify-between mb-3">
        <span className="label">Bookmarks</span>
        <div className="flex gap-2">
          <button onClick={onImportClick} className="text-xs px-2 py-1 rounded border border-border text-secondary hover:text-primary transition-all">Import JSON</button>
          {bookmarks.length > 0 && <button onClick={() => downloadJson(bookmarks, 'explorer-bookmarks.json')} className="text-xs px-2 py-1 rounded border border-border text-secondary hover:text-primary transition-all">Export all</button>}
        </div>
      </div>
      <div className="flex gap-2 mb-3">
        <input value={newName} onChange={e => setNewName(e.target.value)}
          onKeyDown={e => { if (e.key === 'Enter' && newName.trim()) { onSave(newName.trim()); setNewName('') } }}
          placeholder={`Save "${currentTabs.map(t => t.name).join(', ')}"…`}
          className="flex-1 px-2.5 py-1 rounded border border-border bg-raised text-xs text-primary placeholder:text-muted outline-none focus:border-accent/40 transition-colors" />
        <button disabled={!newName.trim()} onClick={() => { onSave(newName.trim()); setNewName('') }}
          className="px-3 py-1 rounded bg-accent/10 border border-accent/20 text-accent text-xs hover:bg-accent/20 transition-all disabled:opacity-40 disabled:cursor-not-allowed">Save</button>
      </div>
      {bookmarks.length === 0
        ? <div className="text-xs text-muted py-1">No bookmarks yet.</div>
        : <div className="space-y-1 max-h-44 overflow-y-auto">
            {bookmarks.map(bm => (
              <div key={bm.id} className="flex items-center gap-2 px-2 py-1.5 rounded hover:bg-raised group transition-colors">
                <div className="flex-1 min-w-0">
                  <div className="text-xs text-primary truncate font-medium">{bm.name}</div>
                  <div className="text-[10px] text-muted">{new Date(bm.savedAt).toLocaleString()} · {bm.tabs.length} tab{bm.tabs.length !== 1 ? 's' : ''}</div>
                </div>
                <button onClick={() => onLoad(bm)} className="text-xs text-accent opacity-0 group-hover:opacity-100 hover:underline transition-all">Load</button>
                <button onClick={() => onExport(bm)} className="text-xs text-secondary opacity-0 group-hover:opacity-100 hover:text-primary transition-all px-1">↓</button>
                <button onClick={() => onDelete(bm.id)} className="text-xs text-muted opacity-0 group-hover:opacity-100 hover:text-down transition-all">×</button>
              </div>
            ))}
          </div>
      }
    </div>
  )
}

// ── Tooltip formatter ──────────────────────────────────────────────────────────

// eslint-disable-next-line @typescript-eslint/no-explicit-any
function axisTooltipFormatter(params: any) {
  const ps = Array.isArray(params) ? params : [params]
  const date = ps[0]?.axisValueLabel ?? ps[0]?.axisValue ?? ''
  const lines = ps
    .filter((p: { seriesName: string }) => !p.seriesName.startsWith('_reg_'))
    .map((p: { color: string; seriesName: string; value: number | null }) =>
      `<span style="display:inline-block;width:8px;height:8px;border-radius:50%;background:${p.color};margin-right:4px"></span>${p.seriesName}: <b>${fmtVal(p.value)}</b>`
    )
  return `<div style="font-size:11px;line-height:1.6"><b>${date}</b><br/>${lines.join('<br/>')}</div>`
}

// ── Single chart card ──────────────────────────────────────────────────────────

function ChartCard({ config, filter, onFilter, onUpdate, onRemove, canRemove }: {
  config:    ChartConfig
  filter:    TabFilter
  onFilter:  (patch: Partial<TabFilter>) => void
  onUpdate:  (patch: Partial<ChartConfig>) => void
  onRemove:  () => void
  canRemove: boolean
}) {
  const { dimension, metric, chartType, aggregation, showRegression } = config
  const [fullscreen, setFullscreen] = useState(false)
  const chartRef = useRef<ReactECharts>(null)

  const isHeatmapDim  = dimension === 'day_hour'
  const effectiveType = isHeatmapDim ? 'heatmap' : chartType
  const isTimeSeries  = effectiveType === 'line'
    && (dimension === 'feature' || dimension === 'feature_type')
    && TIME_SERIES_METRICS.has(metric)
  const showAgg = dimension === 'feature_type' || dimension === 'office'
  const metaMetric = METRICS.find(m => m.id === metric)

  // ── Queries ────────────────────────────────────────────────────────────────
  const frictionQ = useQuery({
    queryKey: ['exFriction', filter.days],
    queryFn:  () => api.frictionHeatmap(filter.days, 100),
    enabled:  dimension === 'feature' || dimension === 'feature_type',
  })
  const officeQ = useQuery({
    queryKey: ['exOffice', filter.days],
    queryFn:  () => api.envFingerprint(filter.days),
    enabled:  dimension === 'office',
  })
  const hourlyQ = useQuery({
    queryKey: ['exHourly', filter.days],
    queryFn:  () => api.hourlyFriction(filter.days),
    enabled:  dimension === 'hour',
  })
  const peakQ = useQuery({
    queryKey: ['exPeak', filter.days, filter.offices[0]],
    queryFn:  () => api.peakHours(filter.days, filter.offices[0] || undefined),
    enabled:  dimension === 'day_hour',
  })

  // ── Time-series groups ─────────────────────────────────────────────────────
  // Each group = { features: string[], label: string }
  // For 'feature': one group per feature (one-to-one)
  // For 'feature_type': one group per type, top 5 features → aggregated
  const timeSeriesGroups = useMemo(() => {
    if (!isTimeSeries) return []
    const rows: FrictionItem[] = frictionQ.data?.data ?? []
    const filt = rows.filter(r =>
      (filter.featureTypes.length === 0 || filter.featureTypes.includes(r.feature_type)) &&
      (filter.features.length === 0     || filter.features.includes(r.feature))
    )
    if (dimension === 'feature') {
      const feats = filter.features.length > 0
        ? filter.features.slice(0, 8)
        : [...filt].sort((a, b) => frictionVal(b, metric) - frictionVal(a, metric)).slice(0, 5).map(r => r.feature)
      return feats.map(f => ({ features: [f], label: f }))
    }
    if (dimension === 'feature_type') {
      const byType = new Map<string, string[]>()
      filt.forEach(r => { if (!byType.has(r.feature_type)) byType.set(r.feature_type, []); byType.get(r.feature_type)!.push(r.feature) })
      return [...byType.entries()].map(([type, feats]) => {
        const top = feats
          .map(f => filt.find(r => r.feature === f)!)
          .sort((a, b) => frictionVal(b, metric) - frictionVal(a, metric))
          .slice(0, 5).map(r => r.feature)
        return { features: top, label: type }
      })
    }
    return []
  }, [isTimeSeries, frictionQ.data, filter, dimension, metric])

  const allTsFeatures = useMemo(() => [...new Set(timeSeriesGroups.flatMap(g => g.features))], [timeSeriesGroups])

  const trendQueries = useQueries({
    queries: allTsFeatures.map(f => ({
      queryKey: ['trend', f, filter.days],
      queryFn:  () => api.featureTrend(f, filter.days),
    })),
  })

  const trendByFeature = useMemo(() => {
    const m = new Map<string, TrendDay[]>()
    allTsFeatures.forEach((f, i) => { m.set(f, trendQueries[i]?.data?.data ?? []) })
    return m
  }, [allTsFeatures, trendQueries])

  // ── Chart option ───────────────────────────────────────────────────────────
  const chartOption = useMemo(() => {

    // ── Time series ────────────────────────────────────────────────────────
    if (isTimeSeries) {
      if (timeSeriesGroups.length === 0) return null
      const allDates = [...new Set(
        [...trendByFeature.values()].flatMap(rows => rows.map((d: TrendDay) => String(d.stat_date)))
      )].sort()

      const mainSeries = timeSeriesGroups.map((group, i) => {
        const color = PALETTE[i % PALETTE.length]
        const data = allDates.map(date => {
          const vals = group.features
            .map(f => { const row = trendByFeature.get(f)?.find((d: TrendDay) => String(d.stat_date) === date); return row ? trendVal(row, metric) : null })
            .filter((v): v is number => v !== null)
          return vals.length > 0 ? applyAgg(vals, aggregation) : null
        })
        return { name: group.label, type: 'line', smooth: false, showSymbol: false, connectNulls: false, data, lineStyle: { color, width: 2 }, itemStyle: { color } }
      })

      const regSeries = showRegression
        ? mainSeries.map((s, i) => {
            const color = PALETTE[i % PALETTE.length]
            return {
              name: `_reg_${s.name}`,
              type: 'line', smooth: false, showSymbol: false, silent: true,
              data: linReg(s.data as (number | null)[]),
              lineStyle: { color, width: 1, type: 'dashed', opacity: 0.6 },
              itemStyle: { color }, legendHoverLink: false,
            }
          })
        : []

      return {
        grid: { top: 28, right: 150, bottom: 36, left: 60 },
        xAxis: { type: 'category', data: allDates.map(d => d.slice(5)), axisLabel: { fontSize: 9 } },
        yAxis: { type: 'value', name: metaMetric?.short, nameTextStyle: { color: '#737373', fontSize: 9 }, axisLabel: { fontSize: 9 } },
        legend: { right: 4, top: 2, type: 'scroll', textStyle: { color: '#737373', fontSize: 9 },
          formatter: (name: string) => name.startsWith('_reg_') ? '' : name },
        series: [...mainSeries, ...regSeries],
        tooltip: { trigger: 'axis', formatter: axisTooltipFormatter },
      }
    }

    // ── Day × Hour heatmap ─────────────────────────────────────────────────
    if (dimension === 'day_hour') {
      const rows = peakQ.data?.data ?? []
      const DAYS = ['Sun', 'Mon', 'Tue', 'Wed', 'Thu', 'Fri', 'Sat']
      const maxC = Math.max(...rows.map((d: PeakHourRow) => d.event_count), 1)
      return {
        grid: { top: 16, right: 24, bottom: 52, left: 44 },
        xAxis: { type: 'category', data: Array.from({ length: 24 }, (_, i) => `${i}h`), axisLabel: { fontSize: 9, interval: 1 } },
        yAxis: { type: 'category', data: DAYS, axisLabel: { fontSize: 10 } },
        visualMap: { min: 0, max: maxC, show: true, orient: 'horizontal', bottom: 4, left: 'center', textStyle: { color: '#737373', fontSize: 9 }, inRange: { color: ['#f5f3ff','#a5b4fc','#4f46e5'] } },
        series: [{ type: 'heatmap', data: rows.map((d: PeakHourRow) => [d.hour_utc, d.day_of_week, d.event_count]) }],
        tooltip: { trigger: 'item', formatter: (p: Record<string, unknown>) => {
          const v = p.value as [number, number, number]
          return `<b>${DAYS[v[1]]} ${v[0]}:00</b><br/>${v[2].toLocaleString()} events`
        }},
      }
    }

    // ── Hour of Day ────────────────────────────────────────────────────────
    if (dimension === 'hour') {
      const rows = (hourlyQ.data?.data ?? []) as Record<string, number>[]
      const mKey = metric === 'event_count' ? 'event_count' : metric === 'avg_ms' ? 'avg_ms' : 'p95_ms'
      const sorted = [...rows].sort((a, b) => a.hour_utc - b.hour_utc)
      const data = sorted.map(d => +(d[mKey] ?? 0).toFixed(1))
      const series: object[] = [{ type: effectiveType === 'line' ? 'line' : 'bar', data, smooth: false, showSymbol: false, itemStyle: { color: '#6366f1' }, areaStyle: effectiveType === 'line' ? { color: 'rgba(99,102,241,0.07)' } : undefined, barMaxWidth: 28 }]
      if (showRegression && effectiveType === 'line') series.push({ name: '_reg_', type: 'line', data: linReg(data), smooth: false, showSymbol: false, silent: true, lineStyle: { color: '#6366f1', width: 1, type: 'dashed', opacity: 0.5 }, itemStyle: { color: '#6366f1' } })
      return {
        grid: { top: 20, right: 20, bottom: 36, left: 58 },
        xAxis: { type: 'category', data: sorted.map(d => `${d.hour_utc}h`), axisLabel: { fontSize: 9 } },
        yAxis: { type: 'value', name: metaMetric?.short, nameTextStyle: { color: '#737373', fontSize: 9 }, axisLabel: { fontSize: 9 } },
        series,
        tooltip: { trigger: 'axis', formatter: axisTooltipFormatter },
      }
    }

    // ── Office ─────────────────────────────────────────────────────────────
    if (dimension === 'office') {
      const rows: OfficeRow[] = officeQ.data?.data ?? []
      const filt = rows.filter(r =>
        (filter.featureTypes.length === 0 || filter.featureTypes.includes(r.feature_type)) &&
        (filter.features.length === 0     || filter.features.includes(r.feature))
      )
      const byOffice = new Map<string, number[]>()
      filt.forEach(r => {
        if (!byOffice.has(r.office_prefix)) byOffice.set(r.office_prefix, [])
        const val = (metric === 'avg_ms' ? r.avg_ms : metric === 'p95_ms' ? r.p95_ms : r.event_count) as number | null
        if (val != null) byOffice.get(r.office_prefix)!.push(val)
      })
      const entries = [...byOffice.entries()]
        .map(([office, vals]) => ({ office, value: applyAgg(vals, aggregation) }))
        .sort((a, b) => b.value - a.value)

      if (effectiveType === 'scatter') {
        return {
          grid: { top: 20, right: 20, bottom: 36, left: 62 },
          xAxis: { type: 'value', name: 'Total Events', nameTextStyle: { color: '#737373', fontSize: 9 }, axisLabel: { fontSize: 9 } },
          yAxis: { type: 'value', name: metaMetric?.short, nameTextStyle: { color: '#737373', fontSize: 9 }, axisLabel: { fontSize: 9 } },
          series: [{ type: 'scatter', symbolSize: 18,
            data: entries.map(e => [filt.filter(r => r.office_prefix === e.office).reduce((a, r) => a + r.event_count, 0), +e.value.toFixed(1), e.office]),
            itemStyle: { color: (p: { value: unknown[] }) => OFFICE_COLOR[p.value[2] as string] ?? '#6366f1' },
            label: { show: true, formatter: (p: { value: unknown[] }) => String(p.value[2]), fontSize: 10, position: 'top', color: '#a3a3a3' },
          }],
          tooltip: { trigger: 'item', formatter: (p: Record<string, unknown>) => { const v = p.value as [number, number, string]; return `<b>${v[2]}</b><br/>Events: ${v[0].toLocaleString()}<br/>${metaMetric?.short}: ${fmtVal(v[1])}` }},
        }
      }
      return {
        grid: { top: 20, right: 20, bottom: 36, left: 62 },
        xAxis: { type: 'category', data: entries.map(e => e.office), axisLabel: { fontSize: 11 } },
        yAxis: { type: 'value', name: metaMetric?.short, nameTextStyle: { color: '#737373', fontSize: 9 }, axisLabel: { fontSize: 9 } },
        series: [{ type: effectiveType === 'line' ? 'line' : 'bar', data: entries.map(e => +e.value.toFixed(1)), itemStyle: { color: (p: { dataIndex: number }) => OFFICE_COLOR[entries[p.dataIndex]?.office] ?? '#6366f1' }, barMaxWidth: 64, smooth: false, showSymbol: false, label: { show: true, position: 'top', fontSize: 9, color: '#a3a3a3' } }],
        tooltip: { trigger: 'axis', formatter: axisTooltipFormatter },
      }
    }

    // ── Feature type (bar/scatter) ─────────────────────────────────────────
    const frRows: FrictionItem[] = frictionQ.data?.data ?? []
    const filt = frRows.filter(r =>
      (filter.featureTypes.length === 0 || filter.featureTypes.includes(r.feature_type)) &&
      (filter.features.length === 0     || filter.features.includes(r.feature))
    )

    if (dimension === 'feature_type') {
      const byType = new Map<string, number[]>()
      filt.forEach(r => { if (!byType.has(r.feature_type)) byType.set(r.feature_type, []); byType.get(r.feature_type)!.push(frictionVal(r, metric)) })
      const entries = [...byType.entries()]
        .map(([type, vals], i) => ({ type, value: applyAgg(vals, aggregation), color: PALETTE[i % PALETTE.length] }))
        .sort((a, b) => b.value - a.value)

      if (effectiveType === 'scatter') {
        return {
          grid: { top: 20, right: 20, bottom: 36, left: 62 },
          xAxis: { type: 'value', name: 'Events', nameTextStyle: { color: '#737373', fontSize: 9 }, axisLabel: { fontSize: 9 } },
          yAxis: { type: 'value', name: metaMetric?.short, nameTextStyle: { color: '#737373', fontSize: 9 }, axisLabel: { fontSize: 9 } },
          series: [{ type: 'scatter', symbolSize: 14,
            data: entries.map(e => [filt.filter(r => r.feature_type === e.type).reduce((a, r) => a + r.total_events, 0), +e.value.toFixed(2), e.type]),
            itemStyle: { color: (p: { dataIndex: number }) => entries[p.dataIndex]?.color ?? '#6366f1' },
            label: { show: true, formatter: (p: { value: unknown[] }) => String(p.value[2]), fontSize: 9, position: 'top', color: '#a3a3a3' },
          }],
          tooltip: { trigger: 'item', formatter: (p: Record<string, unknown>) => { const v = p.value as [number, number, string]; return `<b>${v[2]}</b><br/>Events: ${v[0].toLocaleString()}<br/>${metaMetric?.short}: ${fmtVal(v[1])}` }},
        }
      }
      return {
        grid: { top: 20, right: 20, bottom: entries.length > 6 ? 60 : 36, left: 62 },
        xAxis: { type: 'category', data: entries.map(e => e.type), axisLabel: { fontSize: 10, rotate: entries.length > 6 ? 15 : 0 } },
        yAxis: { type: 'value', name: metaMetric?.short, nameTextStyle: { color: '#737373', fontSize: 9 }, axisLabel: { fontSize: 9 } },
        series: [{ type: 'bar', data: entries.map(e => +e.value.toFixed(2)), itemStyle: { color: (p: { dataIndex: number }) => entries[p.dataIndex]?.color ?? '#6366f1' }, barMaxWidth: 64, label: { show: true, position: 'top', fontSize: 9, color: '#a3a3a3' } }],
        tooltip: { trigger: 'axis', formatter: axisTooltipFormatter },
      }
    }

    // ── Feature (bar/scatter) ──────────────────────────────────────────────
    const top = [...filt].sort((a, b) => frictionVal(b, metric) - frictionVal(a, metric)).slice(0, 40)

    if (effectiveType === 'scatter') {
      return {
        grid: { top: 20, right: 20, bottom: 36, left: 62 },
        xAxis: { type: 'value', name: 'Event Count', nameTextStyle: { color: '#737373', fontSize: 9 }, axisLabel: { fontSize: 9 } },
        yAxis: { type: 'value', name: metaMetric?.short, nameTextStyle: { color: '#737373', fontSize: 9 }, axisLabel: { fontSize: 9 } },
        series: [{ type: 'scatter', symbolSize: 7, data: top.map(r => [r.total_events, +frictionVal(r, metric).toFixed(1), r.feature]), itemStyle: { color: '#6366f1', opacity: 0.65 } }],
        tooltip: { trigger: 'item', formatter: (p: Record<string, unknown>) => { const v = p.value as [number, number, string]; return `<b>${v[2]}</b><br/>Events: ${v[0].toLocaleString()}<br/>${metaMetric?.short}: ${fmtVal(v[1])}` }},
      }
    }
    return {
      grid: { top: 20, right: 20, bottom: 88, left: 62 },
      xAxis: { type: 'category', data: top.map(r => r.feature), axisLabel: { fontSize: 8, rotate: 30, interval: 0 } },
      yAxis: { type: 'value', name: metaMetric?.short, nameTextStyle: { color: '#737373', fontSize: 9 }, axisLabel: { fontSize: 9 } },
      series: [{ type: 'bar', data: top.map(r => +frictionVal(r, metric).toFixed(2)), itemStyle: { color: (p: { dataIndex: number }) => PALETTE[p.dataIndex % PALETTE.length], opacity: 0.9 }, barMaxWidth: 22 }],
      tooltip: { trigger: 'axis', formatter: axisTooltipFormatter },
    }
  }, [isTimeSeries, dimension, metric, effectiveType, aggregation, showRegression, filter, metaMetric,
      frictionQ.data, officeQ.data, hourlyQ.data, peakQ.data, timeSeriesGroups, trendByFeature])

  const handleClick = useCallback((params: unknown) => {
    const p = params as Record<string, unknown>
    const name = typeof p.name === 'string' ? p.name : ''
    if (!name) return
    if (dimension === 'office')            onFilter({ offices:      [name] })
    else if (dimension === 'feature_type') onFilter({ featureTypes: [name] })
    else if (dimension === 'feature')      onFilter({ features:     [name] })
  }, [dimension, onFilter])

  const exportPng = useCallback(() => {
    const instance = chartRef.current?.getEchartsInstance()
    if (!instance) return
    const url = instance.getDataURL({ type: 'png', pixelRatio: 2 })
    Object.assign(document.createElement('a'), { href: url, download: `chart-${dimension}-${metric}.png` }).click()
  }, [dimension, metric])

  const staticLoading = frictionQ.isLoading || officeQ.isLoading || hourlyQ.isLoading || peakQ.isLoading
  const tsLoading     = isTimeSeries && (frictionQ.isLoading || trendQueries.some(q => q.isLoading))
  const isLoading     = isTimeSeries ? tsLoading : staticLoading
  const noTsMetric    = effectiveType === 'line' && (dimension === 'feature' || dimension === 'feature_type') && !TIME_SERIES_METRICS.has(metric)

  const header = (onClose?: () => void) => (
    <div className="flex items-center gap-2 px-3 pt-2.5 pb-2 border-b border-border flex-wrap flex-shrink-0">
      <select value={dimension} onChange={e => onUpdate({ dimension: e.target.value as DimId })}
        className="px-2 py-0.5 rounded border border-border bg-surface text-xs text-primary outline-none cursor-pointer">
        {DIMENSIONS.map(d => <option key={d.id} value={d.id}>{d.label}</option>)}
      </select>

      <select value={metric} onChange={e => onUpdate({ metric: e.target.value as MetricId })} disabled={isHeatmapDim}
        className="px-2 py-0.5 rounded border border-border bg-surface text-xs text-primary outline-none cursor-pointer disabled:opacity-40">
        {METRICS.map(m => <option key={m.id} value={m.id}>{m.label}</option>)}
      </select>

      <div className="flex items-center rounded border border-border overflow-hidden">
        {CHART_TYPES.map((ct, i) => {
          const disabled = isHeatmapDim && ct.id !== 'heatmap'
          return (
            <button key={ct.id} disabled={disabled} onClick={() => onUpdate({ chartType: ct.id as ChartTypeId })}
              className={`px-2 py-0.5 text-xs transition-all ${i > 0 ? 'border-l border-border' : ''} ${effectiveType === ct.id ? 'bg-accent/10 text-accent' : disabled ? 'text-muted cursor-not-allowed' : 'text-secondary hover:text-primary hover:bg-raised'}`}>
              {ct.label}
            </button>
          )
        })}
      </div>

      {showAgg && (
        <select value={aggregation} onChange={e => onUpdate({ aggregation: e.target.value as AggId })}
          className="px-2 py-0.5 rounded border border-border bg-surface text-xs text-primary outline-none cursor-pointer">
          {AGGREGATIONS.map(a => <option key={a.id} value={a.id}>{a.label}</option>)}
        </select>
      )}

      {(isTimeSeries || (effectiveType === 'line' && dimension === 'hour')) && (
        <button onClick={() => onUpdate({ showRegression: !showRegression })}
          className={`px-2 py-0.5 rounded text-xs border transition-all ${showRegression ? 'border-accent/30 bg-accent/10 text-accent' : 'border-border text-muted hover:text-secondary'}`}
          title="Linear regression overlay">
          Trend
        </button>
      )}

      <div className="ml-auto flex items-center gap-1">
        {/* Export PNG */}
        <button onClick={exportPng} title="Export PNG"
          className="text-muted hover:text-primary transition-colors p-0.5 rounded hover:bg-raised">
          <svg viewBox="0 0 14 14" fill="none" stroke="currentColor" strokeWidth="1.4" className="w-3.5 h-3.5">
            <path d="M7 1v8M4 6l3 3 3-3M2 10v2a1 1 0 001 1h8a1 1 0 001-1v-2"/>
          </svg>
        </button>
        {/* Fullscreen toggle */}
        {onClose
          ? <button onClick={onClose} title="Exit fullscreen" className="text-muted hover:text-primary transition-colors p-0.5 rounded hover:bg-raised">
              <svg viewBox="0 0 14 14" fill="none" stroke="currentColor" strokeWidth="1.4" className="w-3.5 h-3.5">
                <path d="M5 1H1v4M9 1h4v4M5 13H1v-4M9 13h4v-4"/>
              </svg>
            </button>
          : <button onClick={() => setFullscreen(true)} title="Fullscreen" className="text-muted hover:text-primary transition-colors p-0.5 rounded hover:bg-raised">
              <svg viewBox="0 0 14 14" fill="none" stroke="currentColor" strokeWidth="1.4" className="w-3.5 h-3.5">
                <path d="M1 5V1h4M9 1h4v4M13 9v4H9M5 13H1V9"/>
              </svg>
            </button>
        }
        {canRemove && !onClose && (
          <button onClick={onRemove} title="Remove chart" className="text-muted hover:text-down transition-colors px-1 text-base leading-none">×</button>
        )}
      </div>
    </div>
  )

  const body = (height: number) => (
    noTsMetric
      ? <div style={{ height }} className="flex flex-col items-center justify-center gap-1 text-center">
          <span className="text-xs text-secondary">Time series unavailable for <b>{metaMetric?.label}</b></span>
          <span className="text-[11px] text-muted">Switch to Avg ms, P95 ms, or Event Count</span>
        </div>
      : isLoading
        ? <div style={{ height }} className="flex items-center justify-center"><PageSpinner /></div>
        : !chartOption
          ? <div style={{ height }} className="flex items-center justify-center text-xs text-muted">No data</div>
          : <Chart ref={chartRef} option={chartOption} height={height} onEvents={{ click: handleClick }} />
  )

  return (
    <>
      <div className="card flex flex-col">
        {header()}
        <div className="p-3">{body(300)}</div>
      </div>

      {fullscreen && createPortal(
        <div className="fixed inset-0 z-[9999] bg-surface flex flex-col" style={{ padding: '1rem' }}>
          <div className="card flex flex-col flex-1 min-h-0">
            {header(() => setFullscreen(false))}
            <div className="flex-1 p-3 min-h-0">{body(window.innerHeight - 120)}</div>
          </div>
        </div>,
        document.body
      )}
    </>
  )
}

// ── Main Explorer ──────────────────────────────────────────────────────────────

export default function Explorer() {
  const [tabs,     setTabs]    = useState<TabState[]>(() => sessionLoad()?.tabs ?? [defaultTab()])
  const [activeId, setActiveId] = useState<string>(() => {
    const s = sessionLoad(); return s?.activeId ?? s?.tabs[0]?.id ?? ''
  })
  const [bookmarks,   setBookmarks]   = useState<Bookmark[]>(bmsLoad)
  const [showBms,     setShowBms]     = useState(false)
  const [renamingId,  setRenamingId]  = useState<string | null>(null)
  const [renameValue, setRenameValue] = useState('')
  const fileInputRef = useRef<HTMLInputElement>(null)

  useEffect(() => { sessionSave(tabs, activeId) }, [tabs, activeId])

  const activeTab = tabs.find(t => t.id === activeId) ?? tabs[0]

  const updateTab    = useCallback((id: string, patch: Partial<TabState>) =>
    setTabs(prev => prev.map(t => t.id === id ? { ...t, ...patch } : t)), [])
  const updateFilter = useCallback((id: string, patch: Partial<TabFilter>) =>
    setTabs(prev => prev.map(t => t.id === id ? { ...t, filter: { ...t.filter, ...patch } } : t)), [])

  const addTab = () => { const tab = defaultTab(`Tab ${tabs.length + 1}`); setTabs(p => [...p, tab]); setActiveId(tab.id) }
  const closeTab = (id: string) => {
    const remaining = tabs.filter(t => t.id !== id)
    setTabs(remaining.length === 0 ? [defaultTab()] : remaining)
    if (activeId === id) setActiveId(remaining[0]?.id ?? tabs.find(t => t.id !== id)?.id ?? '')
  }
  const startRename  = (tab: TabState) => { setRenamingId(tab.id); setRenameValue(tab.name) }
  const commitRename = () => { if (renamingId && renameValue.trim()) updateTab(renamingId, { name: renameValue.trim() }); setRenamingId(null) }

  const addChart    = (tid: string) => setTabs(p => p.map(t => t.id === tid ? { ...t, charts: [...t.charts, defaultChart()] } : t))
  const removeChart = (tid: string, cid: string) => setTabs(p => p.map(t => t.id === tid ? { ...t, charts: t.charts.filter(c => c.id !== cid) } : t))
  const updateChart = (tid: string, cid: string, patch: Partial<ChartConfig>) =>
    setTabs(p => p.map(t => t.id === tid ? { ...t, charts: t.charts.map(c => c.id === cid ? { ...c, ...patch } : c) } : t))

  const saveBookmark   = (name: string) => { const bm: Bookmark = { id: uid(), name, tabs, savedAt: new Date().toISOString() }; const u = [...bookmarks, bm]; setBookmarks(u); bmsSave(u) }
  const loadBookmark   = (bm: Bookmark) => { setTabs(bm.tabs); setActiveId(bm.tabs[0]?.id ?? ''); setShowBms(false) }
  const deleteBookmark = (id: string)   => { const u = bookmarks.filter(b => b.id !== id); setBookmarks(u); bmsSave(u) }
  const exportBookmark = (bm: Bookmark) => downloadJson(bm, `bookmark-${bm.name.replace(/\s+/g, '-')}.json`)
  const importBookmark = (e: React.ChangeEvent<HTMLInputElement>) => {
    const file = e.target.files?.[0]; if (!file) return
    const reader = new FileReader()
    reader.onload = ev => { try { const bm = JSON.parse(ev.target?.result as string) as Bookmark; const u = [...bookmarks, { ...bm, id: uid() }]; setBookmarks(u); bmsSave(u) } catch { /* bad json */ } }
    reader.readAsText(file); e.target.value = ''
  }

  const { data: featuresMeta } = useQuery({ queryKey: ['features'], queryFn: api.features })
  const { data: officesMeta  } = useQuery({ queryKey: ['offices'],  queryFn: api.offices  })

  const allFeatureTypes = useMemo(() => [...new Set(featuresMeta?.map(f => f.feature_type) ?? [])], [featuresMeta])
  const allFeatures = useMemo(() =>
    featuresMeta?.filter(f => activeTab.filter.featureTypes.length === 0 || activeTab.filter.featureTypes.includes(f.feature_type)).map(f => f.feature) ?? [],
    [featuresMeta, activeTab.filter.featureTypes])
  const allOffices  = useMemo(() => officesMeta ?? [], [officesMeta])

  const at      = activeTab
  const setFilt = (patch: Partial<TabFilter>) => updateFilter(at.id, patch)
  const chips   = { types: at.filter.featureTypes, features: at.filter.features, offices: at.filter.offices }
  const hasFilters = chips.types.length > 0 || chips.features.length > 0 || chips.offices.length > 0

  return (
    <div className="flex flex-col gap-3 h-full">

      {/* ── Tab bar ── */}
      <div className="flex items-center border-b border-border -mx-1 px-1 flex-shrink-0 overflow-x-auto">
        {tabs.map(tab => (
          <div key={tab.id} className="relative group flex-shrink-0">
            {renamingId === tab.id
              ? <input autoFocus value={renameValue} onChange={e => setRenameValue(e.target.value)}
                  onBlur={commitRename} onKeyDown={e => { if (e.key === 'Enter') commitRename(); if (e.key === 'Escape') setRenamingId(null) }}
                  className="px-2 py-1.5 text-xs border-b-2 border-accent bg-transparent text-primary outline-none w-24" />
              : <button onClick={() => setActiveId(tab.id)} onDoubleClick={() => startRename(tab)} title="Double-click to rename"
                  className={`px-4 py-2 text-xs font-medium border-b-2 transition-all whitespace-nowrap ${tab.id === activeId ? 'border-accent text-accent' : 'border-transparent text-secondary hover:text-primary'}`}>
                  {tab.name}
                </button>
            }
            {tabs.length > 1 && (
              <button onClick={e => { e.stopPropagation(); closeTab(tab.id) }}
                className="absolute right-0.5 top-1.5 hidden group-hover:flex items-center justify-center w-4 h-4 rounded-full hover:bg-raised text-muted hover:text-primary text-[11px] leading-none">×</button>
            )}
          </div>
        ))}
        <button onClick={addTab} className="px-3 py-2 text-sm text-muted hover:text-primary transition-colors flex-shrink-0 leading-none">+</button>
        <div className="ml-auto flex items-center gap-1 pb-0.5 pl-2 flex-shrink-0">
          <button onClick={() => setShowBms(p => !p)}
            className={`flex items-center gap-1.5 px-2.5 py-1 rounded text-xs border transition-all ${showBms ? 'border-accent/30 bg-accent/5 text-accent' : 'border-border text-secondary hover:text-primary'}`}>
            <svg viewBox="0 0 12 14" fill="currentColor" className="w-2.5 h-3 flex-shrink-0"><path d="M1 1h10v12L6 9.5 1 13z"/></svg>
            Bookmarks {bookmarks.length > 0 && <span className="text-[10px] font-mono opacity-60">{bookmarks.length}</span>}
          </button>
        </div>
      </div>

      {showBms && (
        <BookmarkPanel bookmarks={bookmarks} currentTabs={tabs}
          onSave={saveBookmark} onLoad={loadBookmark} onDelete={deleteBookmark}
          onExport={exportBookmark} onImportClick={() => fileInputRef.current?.click()} />
      )}
      <input ref={fileInputRef} type="file" accept=".json" className="hidden" onChange={importBookmark} />

      {/* ── Filter toolbar ── */}
      <div className="flex items-center gap-2 flex-wrap flex-shrink-0">
        <div className="flex items-center rounded border border-border overflow-hidden flex-shrink-0">
          {DAYS_OPTIONS.map((d, i) => (
            <button key={d} onClick={() => setFilt({ days: d })}
              className={`px-2 py-1 text-xs font-mono transition-all ${i > 0 ? 'border-l border-border' : ''} ${at.filter.days === d ? 'bg-raised text-primary' : 'text-muted hover:text-secondary'}`}>
              {d}d
            </button>
          ))}
        </div>
        <div className="w-px h-4 bg-border flex-shrink-0" />
        <MultiSelect label="Type"    options={allFeatureTypes} selected={chips.types}    onChange={v => setFilt({ featureTypes: v, features: [] })} />
        <MultiSelect label="Feature" options={allFeatures}     selected={chips.features} onChange={v => setFilt({ features: v })} />
        <MultiSelect label="Office"  options={allOffices}      selected={chips.offices}  onChange={v => setFilt({ offices: v })} />
        {hasFilters && <button onClick={() => setFilt({ featureTypes: [], features: [], offices: [] })} className="text-xs text-muted hover:text-secondary transition-colors">Clear</button>}
      </div>

      {/* ── Filter chips ── */}
      {hasFilters && (
        <div className="flex items-center gap-1.5 flex-wrap -mt-1 flex-shrink-0">
          {chips.types.map(t    => <Chip key={`t-${t}`} label="type:"    value={t} onClear={() => setFilt({ featureTypes: chips.types.filter(x => x !== t) })} />)}
          {chips.features.map(f => <Chip key={`f-${f}`} label="feature:" value={f} onClear={() => setFilt({ features: chips.features.filter(x => x !== f) })} />)}
          {chips.offices.map(o  => <Chip key={`o-${o}`} label="office:"  value={o} onClear={() => setFilt({ offices: chips.offices.filter(x => x !== o) })} />)}
        </div>
      )}

      {/* ── Chart grid ── */}
      <div className={`grid gap-4 items-start pb-2 ${at.charts.length === 1 ? 'grid-cols-1' : 'grid-cols-1 xl:grid-cols-2'}`}>
        {at.charts.map(chart => (
          <ChartCard key={chart.id} config={chart} filter={at.filter} onFilter={setFilt}
            onUpdate={patch => updateChart(at.id, chart.id, patch)}
            onRemove={() => removeChart(at.id, chart.id)}
            canRemove={at.charts.length > 1} />
        ))}
      </div>

      <div className="-mt-1 pb-4">
        <button onClick={() => addChart(at.id)}
          className="flex items-center gap-1.5 px-3 py-1.5 rounded border border-dashed border-border text-xs text-muted hover:text-primary hover:border-muted transition-all">
          <span className="text-base leading-none">+</span> Add chart
        </button>
      </div>

    </div>
  )
}
