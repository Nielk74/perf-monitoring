import { useState } from 'react'
import { useQuery } from '@tanstack/react-query'
import { useSearchParams } from 'react-router-dom'
import { api, type BlastItem, type DeploymentNight } from '../api/client'
import PageHeader from '../components/PageHeader'
import Chart from '../components/Chart'
import { PageSpinner } from '../components/Spinner'
import ErrorBanner from '../components/ErrorBanner'

function fmtDate(s: string) {
  return new Date(s).toLocaleDateString('en-GB', { day: '2-digit', month: 'short', year: 'numeric' })
}

function deltaColor(d: BlastItem) {
  return d.delta_pct > 10 ? '#ef4444' : d.delta_pct > 3 ? '#f59e0b' : d.delta_pct < -5 ? '#10b981' : '#6366f1'
}

function nightLabel(d: DeploymentNight) {
  return `${d.deployed_date} · ${d.commit_count} commit${d.commit_count > 1 ? 's' : ''} · ${d.files_changed}f +${d.lines_added}/-${d.lines_removed}`
}

export default function BlastRadius() {
  const [searchParams] = useSearchParams()
  const [selectedDate, setSelectedDate] = useState<string>(searchParams.get('date') ?? '')
  const [windowDays, setWindowDays] = useState(14)
  const [zThreshold, setZThreshold] = useState(1.0)

  const { data: deps } = useQuery({ queryKey: ['deployments'], queryFn: () => api.deployments() })
  const selectedNight = deps?.data.find(d => d.deployed_date === selectedDate) ?? null

  const { data, isLoading, error } = useQuery({
    queryKey: ['blastRadius', selectedDate, windowDays, zThreshold],
    queryFn: () => api.blastRadius(selectedDate, windowDays, zThreshold),
    enabled: !!selectedDate,
  })

  const { data: diff } = useQuery({
    queryKey: ['deploymentDiff', selectedDate],
    queryFn: () => api.deploymentDiff(selectedDate),
    enabled: !!selectedDate,
  })

  const { data: hotFiles } = useQuery({
    queryKey: ['hotFiles', windowDays],
    queryFn: () => api.hotFiles(windowDays),
  })

  const { data: authors } = useQuery({
    queryKey: ['authorImpact', windowDays],
    queryFn: () => api.authorImpact(windowDays),
  })

  const barOption = data && data.data.length > 0 && {
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
        description="Before vs after latency for each nightly deployment batch. Select a deployment date to inspect its impact."
        controls={
          <div className="flex items-center gap-2 flex-wrap">
            <select
              className="select"
              value={selectedDate}
              onChange={e => setSelectedDate(e.target.value)}
            >
              <option value="">Choose a deployment night…</option>
              {deps?.data.map(d => (
                <option key={d.deployed_date} value={d.deployed_date}>
                  {nightLabel(d)}
                </option>
              ))}
            </select>
            <select className="select" value={windowDays} onChange={e => setWindowDays(+e.target.value)}>
              {[7, 14, 21, 30, 60, 90, 180, 365].map(v => <option key={v} value={v}>{v}d window</option>)}
            </select>
            <select className="select" value={zThreshold} onChange={e => setZThreshold(+e.target.value)}>
              {[0.5, 1.0, 1.5, 2.0, 2.5].map(v => <option key={v} value={v}>z ≥ {v}</option>)}
            </select>
          </div>
        }
      />

      {!selectedDate && (
        <div className="card p-12 text-center text-secondary text-sm">
          Select a deployment night to analyse its blast radius.
        </div>
      )}
      {error && <ErrorBanner error={error} />}
      {isLoading && <PageSpinner />}

      {/* ── Deployment batch summary ──────────────────────────────────────── */}
      {selectedNight && (
        <div className="card p-4 flex flex-wrap gap-6 text-sm">
          <div>
            <div className="label mb-1">Deployed</div>
            <span className="text-primary font-semibold">{fmtDate(selectedNight.deployed_date)}</span>
          </div>
          <div>
            <div className="label mb-1">Commits</div>
            <span className="text-primary">{selectedNight.commit_count}</span>
          </div>
          <div>
            <div className="label mb-1">Files touched</div>
            <span className="text-primary">{selectedNight.files_changed}</span>
          </div>
          <div>
            <div className="label mb-1">Churn</div>
            <span className="text-up">+{selectedNight.lines_added}</span>
            <span className="text-secondary"> / </span>
            <span className="text-down">-{selectedNight.lines_removed}</span>
          </div>
          <div className="flex-1 min-w-0">
            <div className="label mb-1">Authors</div>
            <span className="text-secondary truncate block">{selectedNight.authors}</span>
          </div>
        </div>
      )}

      {/* ── Blast radius results ──────────────────────────────────────────── */}
      {data && !isLoading && (
        <div className="grid grid-cols-1 lg:grid-cols-5 gap-4">
          <div className="card p-5 lg:col-span-2">
            <div className="label mb-4">Latency delta % — {windowDays}d window</div>
            {data.data.length === 0
              ? <p className="text-sm text-secondary py-8 text-center">No features crossed the z≥{zThreshold} threshold.</p>
              : barOption && <Chart option={barOption} height={Math.max(200, data.data.length * 30 + 32)} />
            }
          </div>
          <div className="card p-5 lg:col-span-3">
            <div className="label mb-4">Affected features</div>
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

      {/* ── Diff: commits + files ─────────────────────────────────────────── */}
      {diff && (
        <div className="grid grid-cols-1 lg:grid-cols-2 gap-4">
          {/* Commits in this batch */}
          <div className="card p-5">
            <div className="label mb-4">
              Commits in this deployment — {diff.meta.commit_count}
            </div>
            <div className="space-y-2 overflow-auto max-h-64">
              {diff.commits.map(c => (
                <div key={c.commit_hash} className="flex items-start gap-3 py-1.5 border-b border-border/50">
                  <span className="font-mono text-xs text-accent flex-shrink-0">{c.commit_hash.slice(0, 7)}</span>
                  <div className="flex-1 min-w-0">
                    <div className="text-xs text-primary truncate">{c.message}</div>
                    <div className="text-xs text-secondary">{c.author_name}</div>
                  </div>
                  <span className="text-xs font-mono flex-shrink-0">
                    <span className="text-up">+{c.lines_added}</span>
                    <span className="text-secondary">/</span>
                    <span className="text-down">-{c.lines_removed}</span>
                  </span>
                </div>
              ))}
            </div>
          </div>

          {/* Files changed (aggregated) */}
          <div className="card p-5">
            <div className="label mb-4">
              Files changed — {diff.meta.file_count}
            </div>
            <div className="overflow-auto max-h-64">
              <table className="w-full text-sm">
                <thead className="sticky top-0 bg-raised">
                  <tr className="border-b border-border">
                    {['File', '', '+', '-'].map(h => (
                      <th key={h} className="text-left py-1.5 pr-3 label">{h}</th>
                    ))}
                  </tr>
                </thead>
                <tbody>
                  {diff.files.map((f, i) => (
                    <tr key={i} className="border-b border-border/50 table-row-hover">
                      <td className="py-1.5 pr-3 font-mono text-xs text-primary">{f.file_path}</td>
                      <td className="py-1.5 pr-3">
                        <span className={`text-xs font-mono ${
                          f.change_type === 'A' ? 'text-up' :
                          f.change_type === 'D' ? 'text-down' : 'text-warn'
                        }`}>{f.change_type}</span>
                      </td>
                      <td className="py-1.5 pr-3 font-mono text-xs text-up">+{f.lines_added}</td>
                      <td className="py-1.5 font-mono text-xs text-down">-{f.lines_removed}</td>
                    </tr>
                  ))}
                </tbody>
              </table>
            </div>
          </div>
        </div>
      )}

      {/* ── Hot files + Author impact ─────────────────────────────────────── */}
      <div className="grid grid-cols-1 lg:grid-cols-2 gap-4">
        <div className="card p-5">
          <div className="label mb-4">Hot files — most frequent in regression deployments</div>
          {!hotFiles?.data.length
            ? <p className="text-sm text-secondary">No regression deployments found.</p>
            : (
              <div className="overflow-auto max-h-64">
                <table className="w-full text-sm">
                  <thead className="sticky top-0 bg-raised">
                    <tr className="border-b border-border">
                      {['File', 'Regression nights', 'Total churn'].map(h => (
                        <th key={h} className="text-left py-2 pr-4 label">{h}</th>
                      ))}
                    </tr>
                  </thead>
                  <tbody>
                    {hotFiles.data.map((f, i) => (
                      <tr key={i} className="border-b border-border/50 table-row-hover">
                        <td className="py-1.5 pr-4 font-mono text-xs text-primary">{f.file_path}</td>
                        <td className="py-1.5 pr-4 font-mono text-down">{f.regression_commits}</td>
                        <td className="py-1.5 font-mono text-xs text-secondary">{f.total_churn_lines.toLocaleString()}</td>
                      </tr>
                    ))}
                  </tbody>
                </table>
              </div>
            )
          }
        </div>

        <div className="card p-5">
          <div className="label mb-4">Author impact — regression rate</div>
          {!authors?.data.length
            ? <p className="text-sm text-secondary">No data available.</p>
            : (
              <div className="overflow-auto max-h-64">
                <table className="w-full text-sm">
                  <thead className="sticky top-0 bg-raised">
                    <tr className="border-b border-border">
                      {['Author', 'Commits', 'Regressions', 'Rate'].map(h => (
                        <th key={h} className="text-left py-2 pr-4 label">{h}</th>
                      ))}
                    </tr>
                  </thead>
                  <tbody>
                    {authors.data.map((a, i) => (
                      <tr key={i} className="border-b border-border/50 table-row-hover">
                        <td className="py-1.5 pr-4 text-primary text-xs">{a.author_name}</td>
                        <td className="py-1.5 pr-4 font-mono text-secondary">{a.total_commits}</td>
                        <td className="py-1.5 pr-4 font-mono text-down">{a.regression_commits}</td>
                        <td className={`py-1.5 font-mono text-xs ${a.regression_rate_pct > 50 ? 'text-down' : a.regression_rate_pct > 20 ? 'text-warn' : 'text-up'}`}>
                          {a.regression_rate_pct}%
                        </td>
                      </tr>
                    ))}
                  </tbody>
                </table>
              </div>
            )
          }
        </div>
      </div>
    </div>
  )
}
