import type { ReactNode } from 'react'

interface KpiCardProps {
  label: string
  value: string | number
  sub?: string
  trend?: 'up' | 'down' | 'neutral'
  icon?: ReactNode
  accent?: boolean
}

export default function KpiCard({ label, value, sub, trend, icon, accent }: KpiCardProps) {
  const trendColor = trend === 'up' ? 'text-up' : trend === 'down' ? 'text-down' : 'text-secondary'
  return (
    <div className={`card p-5 flex flex-col gap-3 ${accent ? 'border-accent/30 shadow-glow' : ''}`}>
      <div className="flex items-center justify-between">
        <span className="label">{label}</span>
        {icon && <span className="text-secondary">{icon}</span>}
      </div>
      <div>
        <div className={`metric-value ${accent ? 'text-accent' : ''}`}>{value}</div>
        {sub && <div className={`text-xs mt-1 ${trendColor}`}>{sub}</div>}
      </div>
    </div>
  )
}
