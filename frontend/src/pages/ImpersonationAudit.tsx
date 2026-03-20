import { useState } from 'react'
import { useQuery } from '@tanstack/react-query'
import { api } from '../api/client'
import PageHeader from '../components/PageHeader'
import Chart from '../components/Chart'
import { PageSpinner } from '../components/Spinner'
import ErrorBanner from '../components/ErrorBanner'

export default function ImpersonationAudit() {
  const [days, setDays] = useState(90)

  const { data: sessions, isLoading: sLoad, error } = useQuery({
    queryKey: ['impersonationSessions', days],
    queryFn: () => api.impersonationSessions(days, 100),
  })
  const { data: agents } = useQuery({
    queryKey: ['supportAgents', days], queryFn: () => api.supportAgents(days),
  })
  const { data: timeline } = useQuery({
    queryKey: ['impersonationTimeline', days], queryFn: () => api.impersonationTimeline(days),
  })

  const timelineOption = {
    grid: { top: 8, right: 8, bottom: 36, left: 52 },
    xAxis: { type: 'category' as const, data: timeline?.data.map((d: Record<string, unknown>) => String(d.day ?? '').slice(5)) ?? [] },
    yAxis: { type: 'value' as const, name: 'Sessions', nameTextStyle: { color: '#6b7280', fontSize: 11 } },
    series: [{
      type: 'bar' as const,
      data: timeline?.data.map((d: Record<string, unknown>) => d.session_count) ?? [],
      itemStyle: { color: '#818cf8', opacity: 0.8 },
    }],
    tooltip: { trigger: 'axis' as const },
  }

  const agentBarOption = agents && {
    grid: { top: 8, right: 60, bottom: 8, left: 8, containLabel: true },
    xAxis: { type: 'value' as const },
    yAxis: { type: 'category' as const, data: [...agents.data].reverse().map(a => a.supp_user) },
    series: [
      { name: 'Sessions', type: 'bar' as const, data: [...agents.data].reverse().map(a => a.session_count), itemStyle: { color: '#818cf8' }, barMaxWidth: 20 },
      { name: 'Users helped', type: 'bar' as const, data: [...agents.data].reverse().map(a => a.distinct_users_helped), itemStyle: { color: '#22d3ee' }, barMaxWidth: 20 },
    ],
    legend: { bottom: 0, textStyle: { color: '#6b7280', fontSize: 11 } },
    tooltip: { trigger: 'axis' as const },
  }

  return (
    <div className="space-y-6 animate-slide-up">
      <PageHeader
        tag="UC-04"
        title="Impersonation Audit"
        description="All support sessions where SUPP_USER ≠ ASMD_USER. Full audit trail for compliance."
        controls={
          <select className="select" value={days} onChange={e => setDays(+e.target.value)}>
            {[30, 60, 90, 180].map(v => <option key={v} value={v}>Last {v}d</option>)}
          </select>
        }
      />

      {error && <ErrorBanner error={error} />}
      {sLoad && <PageSpinner />}

      {!sLoad && (
        <div className="space-y-4">
          {/* Summary KPIs */}
          <div className="grid grid-cols-3 gap-4">
            {[
              { label: 'Total Sessions', value: sessions?.data.length.toLocaleString() ?? '—' },
              { label: 'Support Agents', value: agents?.data.length.toLocaleString() ?? '—' },
              { label: 'Avg Duration', value: agents?.data.length ? (agents.data.reduce((s, a) => s + a.avg_session_minutes, 0) / agents.data.length).toFixed(1) + ' min' : '—' },
            ].map(kpi => (
              <div key={kpi.label} className="card p-4">
                <div className="label mb-2">{kpi.label}</div>
                <div className="text-xl font-semibold text-primary">{kpi.value}</div>
              </div>
            ))}
          </div>

          <div className="grid grid-cols-1 lg:grid-cols-2 gap-4">
            {/* Timeline */}
            <div className="card p-5">
              <div className="label mb-4">Daily session count</div>
              <Chart option={timelineOption} height={220} />
            </div>
            {/* Agent chart */}
            <div className="card p-5">
              <div className="label mb-4">Sessions per agent</div>
              {agentBarOption && <Chart option={agentBarOption} height={Math.max(160, (agents?.data.length ?? 0) * 28 + 40)} />}
            </div>
          </div>

          {/* Sessions table */}
          <div className="card p-5">
            <div className="label mb-4">Recent sessions — {sessions?.data.length} rows</div>
            <div className="overflow-auto max-h-80">
              <table className="w-full text-sm">
                <thead className="sticky top-0 bg-raised">
                  <tr className="border-b border-border">
                    {['User', 'Support agent', 'Workstation', 'Start', 'Duration', 'Events'].map(h => (
                      <th key={h} className="text-left py-2 pr-4 label">{h}</th>
                    ))}
                  </tr>
                </thead>
                <tbody>
                  {sessions?.data.map(s => (
                    <tr key={s.session_id} className="border-b border-border/50 table-row-hover">
                      <td className="py-2 pr-4 text-primary font-medium font-mono text-xs">{s.asmd_user}</td>
                      <td className="py-2 pr-4 text-warn font-mono text-xs">{s.supp_user}</td>
                      <td className="py-2 pr-4 text-secondary font-mono text-xs">{s.workstation}</td>
                      <td className="py-2 pr-4 text-secondary text-xs">{String(s.session_start).slice(0, 16).replace('T', ' ')}</td>
                      <td className="py-2 pr-4 font-mono text-primary text-xs">{s.duration_minutes?.toFixed(1)} min</td>
                      <td className="py-2 font-mono text-secondary text-xs">{s.event_count}</td>
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
