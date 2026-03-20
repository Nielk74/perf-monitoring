import { useState } from 'react'
import { useQuery } from '@tanstack/react-query'
import { api } from '../api/client'
import PageHeader from '../components/PageHeader'
import Chart from '../components/Chart'
import { PageSpinner } from '../components/Spinner'
import ErrorBanner from '../components/ErrorBanner'

export default function WorkflowDiscovery() {
  const [minCount, setMinCount] = useState(5)
  const [ganttUser, setGanttUser] = useState('')
  const [ganttDate, setGanttDate] = useState('2026-01-15')

  const { data, isLoading, error } = useQuery({
    queryKey: ['transitions', minCount],
    queryFn: () => api.transitions(minCount, 60),
  })

  const { data: gantt, isLoading: gLoad } = useQuery({
    queryKey: ['gantt', ganttUser, ganttDate],
    queryFn: () => api.userGantt(ganttUser, ganttDate),
    enabled: !!ganttUser && !!ganttDate,
  })

  // Build Sankey
  const sankyOption = data && data.data.length > 0 && (() => {
    // Step 1: deduplicate bidirectional pairs (keep higher-count direction)
    const edgeMap = new Map<string, { source: string; target: string; value: number }>()
    data.data
      .filter(e => e.from_feature !== e.to_feature)
      .slice(0, 80)
      .forEach(e => {
        const key = [e.from_feature, e.to_feature].sort().join('||')
        const existing = edgeMap.get(key)
        if (!existing || e.transition_count > existing.value) {
          edgeMap.set(key, { source: e.from_feature, target: e.to_feature, value: e.transition_count })
        }
      })
    const allEdges = [...edgeMap.values()]

    // Step 2: Kahn's topological sort with cycle-breaking — assigns ALL nodes to an order
    const nodeNames = [...new Set(allEdges.flatMap(e => [e.source, e.target]))]
    const outWeight = new Map<string, number>()
    allEdges.forEach(e => outWeight.set(e.source, (outWeight.get(e.source) ?? 0) + e.value))
    const inDegree = new Map<string, number>(nodeNames.map(n => [n, 0]))
    const adj = new Map<string, string[]>(nodeNames.map(n => [n, []]))
    allEdges.forEach(e => {
      adj.get(e.source)!.push(e.target)
      inDegree.set(e.target, (inDegree.get(e.target) ?? 0) + 1)
    })
    const topoOrder = new Map<string, number>()
    const runKahn = (queue: string[]) => {
      while (queue.length) {
        const node = queue.shift()!
        if (topoOrder.has(node)) continue
        topoOrder.set(node, topoOrder.size)
        for (const nbr of (adj.get(node) ?? [])) {
          const d = (inDegree.get(nbr) ?? 0) - 1
          inDegree.set(nbr, d)
          if (d === 0 && !topoOrder.has(nbr)) queue.push(nbr)
        }
      }
    }
    // Initial pass
    const seed: string[] = []
    inDegree.forEach((deg, node) => { if (deg === 0) seed.push(node) })
    runKahn(seed)
    // Break remaining cycles: forcibly pick the highest-outflow unresolved node
    while (topoOrder.size < nodeNames.length) {
      const remaining = nodeNames.filter(n => !topoOrder.has(n))
      const breaker = remaining.reduce((best, n) =>
        (outWeight.get(n) ?? 0) > (outWeight.get(best) ?? 0) ? n : best
      )
      topoOrder.set(breaker, topoOrder.size)
      const next: string[] = []
      for (const nbr of (adj.get(breaker) ?? [])) {
        const d = (inDegree.get(nbr) ?? 0) - 1
        inDegree.set(nbr, d)
        if (d === 0 && !topoOrder.has(nbr)) next.push(nbr)
      }
      runKahn(next)
    }

    // Only keep forward edges (source ranked before target) — guarantees DAG
    const links = allEdges.filter(e =>
      topoOrder.get(e.source)! < topoOrder.get(e.target)!
    )
    const nodeSet2 = new Set<string>()
    links.forEach(l => { nodeSet2.add(l.source); nodeSet2.add(l.target) })
    const nodes = [...nodeSet2].map(n => ({ name: n }))
    return {
      series: [{
        type: 'sankey' as const,
        data: nodes,
        links,
        emphasis: { focus: 'adjacency' as const },
        lineStyle: { color: 'gradient' as const, opacity: 0.4 },
        itemStyle: { color: '#6366f1', borderColor: '#6366f1' },
        label: { color: '#1a1a1a', fontSize: 11 },
        nodeGap: 12,
        nodeWidth: 16,
      }],
      tooltip: { trigger: 'item' as const, formatter: (p: { dataType: string; name?: string; data?: { source: string; target: string; value: number } }) => {
        if (p.dataType === 'edge' && p.data) return `${p.data.source} → ${p.data.target}<br/>Count: ${p.data.value}`
        return p.name ?? ''
      }},
    }
  })()

  // Gantt
  const COLORS: Record<string, string> = {
    'Trade Entry': '#6366f1', 'Reporting': '#8b5cf6', 'Search': '#22c55e',
    'Configuration': '#f59e0b', 'Startup': '#f97316', 'Market Data': '#ec4899',
  }
  const ganttFeatures = [...new Set((gantt?.data ?? []).map((d: Record<string, unknown>) => String(d.feature)))]
  const ganttOption = gantt && gantt.data.length > 0 && {
    grid: { top: 8, right: 8, bottom: 36, left: 8, containLabel: true },
    xAxis: { type: 'time' as const, axisLabel: { formatter: (v: number) => new Date(v).toTimeString().slice(0, 5) } },
    yAxis: { type: 'category' as const, data: ganttFeatures, axisLabel: { fontSize: 10 } },
    series: ganttFeatures.map(feat => ({
      name: feat,
      type: 'scatter' as const,
      data: (gantt.data as Record<string, unknown>[])
        .filter(d => d.feature === feat)
        .map(d => [new Date(String(d.mod_dt)).getTime(), feat, d.duration_ms as number]),
      symbolSize: (val: unknown[]) => Math.min(Math.max(((val as [number, string, number])[2] ?? 0) / 20, 4), 18),
      itemStyle: { color: COLORS[feat] ?? '#737373', opacity: 0.8 },
    })),
    tooltip: { trigger: 'item' as const, formatter: (p: { seriesName: string; value: [number, string, number] }) =>
      `<b>${p.seriesName}</b><br/>Time: ${new Date(p.value[0]).toTimeString().slice(0, 8)}<br/>Duration: ${p.value[2]?.toFixed(0) ?? '—'} ms`
    },
  }

  return (
    <div className="space-y-6 animate-slide-up">
      <PageHeader
        tag="UC-07"
        title="Workflow Discovery"
        description="Feature transition patterns from session sequences. Sankey shows most common navigation flows."
        controls={
          <select className="select" value={minCount} onChange={e => setMinCount(+e.target.value)}>
            {[3, 5, 10, 20].map(v => <option key={v} value={v}>Min {v} transitions</option>)}
          </select>
        }
      />

      {error && <ErrorBanner error={error} />}
      {isLoading && <PageSpinner />}

      {data && !isLoading && (
        <div className="space-y-4">
          {/* Sankey */}
          <div className="card p-5">
            <div className="label mb-4">Transition flow — top {Math.min(data.data.length, 40)} edges, min {minCount} count</div>
            {!sankyOption
              ? <p className="text-sm text-secondary py-8 text-center">Not enough data for this threshold.</p>
              : <Chart option={sankyOption} height={380} />
            }
          </div>

          {/* Gantt */}
          <div className="card p-5">
            <div className="label mb-4">User session timeline</div>
            <div className="flex items-center gap-3 mb-4">
              <input
                className="input w-36"
                placeholder="User ID (e.g. U001)"
                value={ganttUser}
                onChange={e => setGanttUser(e.target.value)}
              />
              <input
                type="date"
                className="input"
                value={ganttDate}
                onChange={e => setGanttDate(e.target.value)}
              />
            </div>
            {gLoad && <PageSpinner />}
            {gantt && gantt.data.length === 0 && <p className="text-sm text-secondary">No events for this user/date.</p>}
            {ganttOption && <Chart option={ganttOption} height={Math.max(160, ganttFeatures.length * 32 + 48)} />}
          </div>

          {/* Transition table */}
          <div className="card p-5">
            <div className="label mb-4">Top transitions</div>
            <div className="overflow-auto max-h-72">
              <table className="w-full text-sm">
                <thead className="sticky top-0 bg-raised">
                  <tr className="border-b border-border">
                    {['From', 'To', 'Count', 'Avg gap'].map(h => (
                      <th key={h} className="text-left py-2 pr-4 label">{h}</th>
                    ))}
                  </tr>
                </thead>
                <tbody>
                  {data.data.slice(0, 30).map((row, i) => (
                    <tr key={i} className="border-b border-border/50 table-row-hover">
                      <td className="py-2 pr-4 text-primary text-xs">{row.from_feature}</td>
                      <td className="py-2 pr-4 text-secondary text-xs">{row.to_feature}</td>
                      <td className="py-2 pr-4 font-mono text-accent">{row.transition_count}</td>
                      <td className="py-2 font-mono text-secondary text-xs">{(row.avg_gap_seconds / 60).toFixed(1)} min</td>
                    </tr>
                  ))}
                </tbody>
              </table>
            </div>
          </div>
        </div>
      )}
    </div>
  )
}
