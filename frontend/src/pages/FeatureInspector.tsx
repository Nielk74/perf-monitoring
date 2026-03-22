import { useMemo } from 'react'
import { useSearchParams, useNavigate, Link } from 'react-router-dom'
import { useQuery } from '@tanstack/react-query'
import { api, type DeploymentNight, type DeploymentSummary } from '../api/client'
import PageHeader from '../components/PageHeader'
import Chart from '../components/Chart'
import { PageSpinner } from '../components/Spinner'
import ErrorBanner from '../components/ErrorBanner'

const DAYS_OPTIONS = [7, 14, 30, 60, 90, 180, 365]

export default function FeatureInspector() {
  const [searchParams, setSearchParams] = useSearchParams()
  const navigate = useNavigate()

  const feature = searchParams.get('feature') ?? ''
  const days = Number(searchParams.get('days') ?? '90')

  function setFeature(f: string) { setSearchParams({ feature: f, days: String(days) }) }
  function setDays(d: number)    { setSearchParams({ feature, days: String(d) }) }

  const { data: featuresMeta } = useQuery({ queryKey: ['features'], queryFn: api.features })

  const { data: trend, isLoading, error } = useQuery({
    queryKey: ['featureTrend', feature, days],
    queryFn: () => api.featureTrend(feature, days),
    enabled: !!feature,
  })

  const { data: deps } = useQuery({ queryKey: ['deployments'], queryFn: () => api.deployments() })

  const fromDate = trend?.data[0]?.stat_date ?? ''
  const toDate   = trend?.data[trend.data.length - 1]?.stat_date ?? ''

  const { data: summariesResp } = useQuery({
    queryKey: ['deploymentSummaries', fromDate, toDate],
    queryFn: () => api.deploymentSummaries(fromDate, toDate),
    enabled: !!fromDate && !!toDate,
  })

  // Lookup maps — stable across renders
  const depMap = useMemo(
    () => new Map<string, DeploymentNight>((deps?.data ?? []).map(d => [d.deployed_date, d])),
    [deps],
  )
  const summaryMap = useMemo(
    () => new Map<string, DeploymentSummary>((summariesResp?.data ?? []).map(s => [s.deployed_date, s])),
    [summariesResp],
  )

  const deployDatesInRange = useMemo(
    () => (deps?.data ?? [])
      .filter(d => d.deployed_date >= fromDate && d.deployed_date <= toDate)
      .map(d => d.deployed_date),
    [deps, fromDate, toDate],
  )

  // ── Chart option ────────────────────────────────────────────────────────────
  const option = useMemo(() => {
    if (!trend?.data.length) return null

    const dates  = trend.data.map(d => d.stat_date)
    const p95    = trend.data.map(d => d.p95_ms ?? null)
    const avg    = trend.data.map(d => d.avg_ms  ?? null)
    const volume = trend.data.map(d => d.event_count)

    const labelInterval = Math.max(0, Math.floor(days / 8) - 1)

    return {
      grid: { top: 20, right: 64, bottom: 40, left: 56 },
      xAxis: {
        type: 'category',
        data: dates,
        axisLabel: {
          interval: labelInterval,
          formatter: (v: string) => v.slice(5),
          color: '#737373',
          fontSize: 11,
        },
        axisLine:  { lineStyle: { color: '#333' } },
        splitLine: { show: false },
      },
      yAxis: [
        {
          type: 'value',
          name: 'ms',
          nameTextStyle: { color: '#737373', fontSize: 11 },
          axisLabel: { color: '#737373', fontSize: 11 },
          splitLine: { lineStyle: { color: '#222' } },
        },
        {
          type: 'value',
          name: 'Events',
          nameTextStyle: { color: '#737373', fontSize: 11 },
          axisLabel: { color: '#737373', fontSize: 11 },
          splitLine: { show: false },
        },
      ],
      series: [
        {
          name: 'p95',
          type: 'line',
          yAxisIndex: 0,
          data: p95,
          smooth: true,
          showSymbol: false,
          lineStyle: { color: '#ef4444', type: 'dashed', width: 2 },
          itemStyle: { color: '#ef4444' },
          markLine: {
            symbol: ['none', 'none'],
            silent: false,
            label: { show: false },
            lineStyle: { color: '#818cf8', opacity: 0.35, type: 'dashed', width: 1 },
            emphasis: { lineStyle: { color: '#818cf8', opacity: 0.9, width: 2 } },
            data: deployDatesInRange.map(d => ({ xAxis: d, name: d })),
          },
        },
        {
          name: 'avg',
          type: 'line',
          yAxisIndex: 0,
          data: avg,
          smooth: true,
          showSymbol: false,
          lineStyle: { color: '#6366f1', width: 1.5 },
          itemStyle: { color: '#6366f1' },
        },
        {
          name: 'Volume',
          type: 'bar',
          yAxisIndex: 1,
          data: volume,
          barMaxWidth: 6,
          itemStyle: { color: '#6366f1', opacity: 0.15 },
        },
      ],
      legend: {
        data: ['p95', 'avg', 'Volume'],
        bottom: 0,
        textStyle: { color: '#737373', fontSize: 11 },
      },
      tooltip: {
        trigger: 'axis',
        backgroundColor: '#18181b',
        borderColor: '#27272a',
        borderWidth: 1,
        padding: 10,
        textStyle: { color: '#e4e4e7', fontSize: 12 },
        // eslint-disable-next-line @typescript-eslint/no-explicit-any
        formatter: (params: any[]) => {
          const date = params[0]?.axisValue as string
          let html = `<div style="font-weight:600;margin-bottom:6px">${date}</div>`

          for (const p of params) {
            if (p.value == null) continue
            if (p.seriesName === 'Volume') {
              html += `<div style="display:flex;gap:8px;align-items:center">`
                + `${p.marker}<span style="color:#a1a1aa">Volume</span>`
                + `<span style="font-family:monospace;margin-left:auto">${(p.value as number).toLocaleString()}</span>`
                + `</div>`
            } else {
              html += `<div style="display:flex;gap:8px;align-items:center">`
                + `${p.marker}<span style="color:#a1a1aa">${p.seriesName}</span>`
                + `<span style="font-family:monospace;margin-left:auto">${(p.value as number).toFixed(1)} ms</span>`
                + `</div>`
            }
          }

          const dep = depMap.get(date)
          const sum = summaryMap.get(date)

          if (dep) {
            html += `<div style="border-top:1px solid #27272a;margin:8px 0 6px"></div>`
            html += `<div style="color:#818cf8;font-weight:600;margin-bottom:4px">Deployment</div>`
            html += `<div style="color:#a1a1aa">${dep.commit_count} commit${dep.commit_count > 1 ? 's' : ''}`
              + ` &nbsp;·&nbsp; ${dep.files_changed} files</div>`
            html += `<div style="margin:2px 0"><span style="color:#4ade80">+${dep.lines_added}</span>`
              + ` <span style="color:#71717a">/</span>`
              + ` <span style="color:#f87171">-${dep.lines_removed}</span></div>`
            html += `<div style="color:#71717a;font-size:11px">${dep.authors}</div>`

            if (sum) {
              html += `<div style="border-top:1px solid #27272a;margin:6px 0 4px"></div>`
              if (sum.features_affected > 0) {
                html += `<div style="color:#f87171">`
                  + `${sum.features_affected} feature${sum.features_affected > 1 ? 's' : ''} degraded</div>`
                if (sum.worst_feature) {
                  html += `<div style="color:#fbbf24">Worst: ${sum.worst_feature}`
                    + ` +${sum.worst_delta_pct?.toFixed(1)}%</div>`
                }
              } else {
                html += `<div style="color:#4ade80">No significant impact</div>`
              }
            }

            html += `<div style="color:#52525b;font-size:10px;margin-top:6px;font-style:italic">`
              + `Click to open Blast Radius</div>`
          }

          return html
        },
      },
    }
  }, [trend, deployDatesInRange, depMap, summaryMap])

  // ── Click handler ───────────────────────────────────────────────────────────
  const onChartClick = useMemo(() => ({
    // eslint-disable-next-line @typescript-eslint/no-explicit-any
    click: (params: any) => {
      const date: string | undefined =
        params.componentType === 'markLine'
          ? params.name
          : params.name // category axis value for series clicks

      if (date && depMap.has(date)) {
        navigate(`/blast-radius?date=${date}`)
      }
    },
  }), [depMap, navigate])

  // ── Stats summary ───────────────────────────────────────────────────────────
  const stats = useMemo(() => {
    if (!trend?.data.length) return null
    const p95vals = trend.data.map(d => d.p95_ms).filter(v => v != null) as number[]
    const avgvals = trend.data.map(d => d.avg_ms).filter(v => v != null) as number[]
    const vols    = trend.data.map(d => d.event_count)
    const minP95  = Math.min(...p95vals)
    const maxP95  = Math.max(...p95vals)
    const latestP95 = p95vals[p95vals.length - 1] ?? null
    const avgAvg  = avgvals.reduce((a, b) => a + b, 0) / (avgvals.length || 1)
    const totalVol = vols.reduce((a, b) => a + b, 0)
    return { minP95, maxP95, latestP95, avgAvg, totalVol }
  }, [trend])

  return (
    <div className="space-y-6 animate-slide-up">
      <PageHeader
        tag=""
        title="Feature Inspector"
        description="p95 &amp; avg latency over time with nightly deployment markers. Click a marker to open its Blast Radius."
        controls={
          <div className="flex items-center gap-2 flex-wrap">
            <select
              className="select"
              value={feature}
              onChange={e => setFeature(e.target.value)}
            >
              <option value="">Choose a feature…</option>
              {featuresMeta?.map(f => (
                <option key={f.feature} value={f.feature}>
                  [{f.feature_type}] {f.feature}
                </option>
              ))}
            </select>
            <select className="select" value={days} onChange={e => setDays(+e.target.value)}>
              {DAYS_OPTIONS.map(v => <option key={v} value={v}>{v}d</option>)}
            </select>
          </div>
        }
      />

      {!feature && (
        <div className="card p-12 text-center text-secondary text-sm">
          Select a feature to inspect its latency trend.
        </div>
      )}

      {error && <ErrorBanner error={error} />}
      {isLoading && <PageSpinner />}

      {/* ── KPI strip ─────────────────────────────────────────────────────── */}
      {stats && (
        <div className="grid grid-cols-2 lg:grid-cols-5 gap-3">
          {[
            { label: 'Current p95',  value: `${stats.latestP95?.toFixed(0) ?? '—'} ms`,  accent: true },
            { label: 'Min p95',      value: `${stats.minP95.toFixed(0)} ms` },
            { label: 'Max p95',      value: `${stats.maxP95.toFixed(0)} ms` },
            { label: 'Avg latency',  value: `${stats.avgAvg.toFixed(0)} ms` },
            { label: 'Total events', value: stats.totalVol.toLocaleString() },
          ].map(k => (
            <div key={k.label} className={`card p-4 ${k.accent ? 'border-accent/30' : ''}`}>
              <div className="label mb-1">{k.label}</div>
              <div className={`text-lg font-semibold font-mono ${k.accent ? 'text-accent' : 'text-primary'}`}>
                {k.value}
              </div>
            </div>
          ))}
        </div>
      )}

      {/* ── Main chart ────────────────────────────────────────────────────── */}
      {option && (
        <div className="card p-5">
          <div className="flex items-center justify-between mb-4">
            <div className="label">{feature} — {days}d trend</div>
            <div className="flex items-center gap-3 text-xs text-secondary">
              <span className="flex items-center gap-1">
                <span className="inline-block w-6 border-t-2 border-dashed border-[#818cf8] opacity-60" />
                Deployment night
              </span>
              <span className="flex items-center gap-1">
                <span className="inline-block w-6 border-t-2 border-dashed border-[#ef4444]" />
                p95
              </span>
              <span className="flex items-center gap-1">
                <span className="inline-block w-6 border-t border-[#6366f1]" />
                avg
              </span>
            </div>
          </div>
          <Chart option={option} height={320} onEvents={onChartClick} />
          <p className="text-xs text-secondary mt-3 text-center">
            Hover deployment markers for batch details &amp; blast radius summary &nbsp;·&nbsp; Click to open full Blast Radius
          </p>
        </div>
      )}

      {/* ── Daily table ───────────────────────────────────────────────────── */}
      {trend?.data && trend.data.length > 0 && (
        <div className="card p-5">
          <div className="label mb-4">Daily stats</div>
          <div className="overflow-auto max-h-64">
            <table className="w-full text-sm">
              <thead className="sticky top-0 bg-raised">
                <tr className="border-b border-border">
                  {['Date', 'p95 ms', 'avg ms', 'p50 ms', 'Events', 'Users', 'Deploy?'].map(h => (
                    <th key={h} className="text-left py-2 pr-4 label">{h}</th>
                  ))}
                </tr>
              </thead>
              <tbody>
                {[...trend.data].reverse().map(row => {
                  const dep = depMap.get(row.stat_date)
                  const sum = summaryMap.get(row.stat_date)
                  return (
                    <tr
                      key={row.stat_date}
                      className={`border-b border-border/50 table-row-hover ${dep ? 'bg-accent/5' : ''}`}
                    >
                      <td className="py-1.5 pr-4 font-mono text-xs text-secondary">{row.stat_date}</td>
                      <td className="py-1.5 pr-4 font-mono text-warn">{row.p95_ms?.toFixed(0) ?? '—'}</td>
                      <td className="py-1.5 pr-4 font-mono text-primary">{row.avg_ms?.toFixed(0) ?? '—'}</td>
                      <td className="py-1.5 pr-4 font-mono text-secondary">{row.p50_ms?.toFixed(0) ?? '—'}</td>
                      <td className="py-1.5 pr-4 font-mono text-secondary">{row.event_count.toLocaleString()}</td>
                      <td className="py-1.5 pr-4 font-mono text-secondary">{row.distinct_users}</td>
                      <td className="py-1.5">
                        {dep ? (
                          <Link
                            to={`/blast-radius?date=${row.stat_date}`}
                            className="text-xs text-accent hover:underline font-mono"
                            title={`${dep.commit_count} commits${sum?.features_affected ? ` · ${sum.features_affected} features affected` : ''}`}
                          >
                            {dep.commit_count}c
                            {sum && sum.features_affected > 0 && (
                              <span className="text-down ml-1">↑{sum.features_affected}</span>
                            )}
                          </Link>
                        ) : (
                          <span className="text-muted">—</span>
                        )}
                      </td>
                    </tr>
                  )
                })}
              </tbody>
            </table>
          </div>
        </div>
      )}
    </div>
  )
}
