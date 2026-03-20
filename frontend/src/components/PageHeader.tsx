import type { ReactNode } from 'react'

interface PageHeaderProps {
  tag: string
  title: string
  description: string
  controls?: ReactNode
}

export default function PageHeader({ tag, title, description, controls }: PageHeaderProps) {
  return (
    <div className="flex items-start justify-between gap-6 mb-6">
      <div className="flex items-start gap-3">
        <span className="font-mono text-xs text-accent/60 mt-1 leading-none">{tag}</span>
        <div>
          <h2 className="text-lg font-semibold text-primary leading-none">{title}</h2>
          <p className="text-sm text-secondary mt-1">{description}</p>
        </div>
      </div>
      {controls && <div className="flex items-center gap-2 flex-shrink-0">{controls}</div>}
    </div>
  )
}
