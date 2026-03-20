import { NavLink, useLocation } from 'react-router-dom'
import { useState, type ReactNode } from 'react'

interface NavItem {
  path: string
  label: string
  tag: string
  icon: ReactNode
}

const NAV: NavItem[] = [
  {
    path: '/', label: 'Overview', tag: '',
    icon: <svg viewBox="0 0 16 16" fill="none" className="w-4 h-4" stroke="currentColor" strokeWidth="1.5">
      <rect x="1" y="1" width="5.5" height="5.5" rx="1"/><rect x="9.5" y="1" width="5.5" height="5.5" rx="1"/>
      <rect x="1" y="9.5" width="5.5" height="5.5" rx="1"/><rect x="9.5" y="9.5" width="5.5" height="5.5" rx="1"/>
    </svg>,
  },
  {
    path: '/explorer', label: 'Explorer', tag: '',
    icon: <svg viewBox="0 0 16 16" fill="none" className="w-4 h-4" stroke="currentColor" strokeWidth="1.5">
      <circle cx="6.5" cy="6.5" r="4.5"/><line x1="10" y1="10" x2="14.5" y2="14.5"/>
      <line x1="4.5" y1="6.5" x2="8.5" y2="6.5"/><line x1="6.5" y1="4.5" x2="6.5" y2="8.5"/>
    </svg>,
  },
  {
    path: '/silent-degrader', label: 'Silent Degrader', tag: '01',
    icon: <svg viewBox="0 0 16 16" fill="none" className="w-4 h-4" stroke="currentColor" strokeWidth="1.5">
      <polyline points="1,4 5,10 9,7 15,13"/><line x1="12" y1="13" x2="15" y2="13"/><line x1="15" y1="10" x2="15" y2="13"/>
    </svg>,
  },
  {
    path: '/blast-radius', label: 'Blast Radius', tag: '02',
    icon: <svg viewBox="0 0 16 16" fill="none" className="w-4 h-4" stroke="currentColor" strokeWidth="1.5">
      <circle cx="8" cy="8" r="2.5"/><line x1="8" y1="1" x2="8" y2="4"/><line x1="8" y1="12" x2="8" y2="15"/>
      <line x1="1" y1="8" x2="4" y2="8"/><line x1="12" y1="8" x2="15" y2="8"/>
      <line x1="3" y1="3" x2="5" y2="5"/><line x1="11" y1="11" x2="13" y2="13"/>
      <line x1="13" y1="3" x2="11" y2="5"/><line x1="5" y1="11" x2="3" y2="13"/>
    </svg>,
  },
  {
    path: '/environmental-fingerprint', label: 'Env Fingerprint', tag: '03',
    icon: <svg viewBox="0 0 16 16" fill="none" className="w-4 h-4" stroke="currentColor" strokeWidth="1.5">
      <circle cx="8" cy="8" r="6.5"/><ellipse cx="8" cy="8" rx="2.5" ry="6.5"/>
      <line x1="1.5" y1="8" x2="14.5" y2="8"/>
    </svg>,
  },
  {
    path: '/impersonation-audit', label: 'Impersonation', tag: '04',
    icon: <svg viewBox="0 0 16 16" fill="none" className="w-4 h-4" stroke="currentColor" strokeWidth="1.5">
      <circle cx="6" cy="5" r="2.5"/><path d="M1 13c0-2.8 2.2-5 5-5"/>
      <circle cx="12" cy="5" r="2.5" strokeDasharray="2 1.5"/><path d="M10 8.2a5 5 0 0 1 5 4.8" strokeDasharray="2 1.5"/>
    </svg>,
  },
  {
    path: '/feature-sunset', label: 'Feature Sunset', tag: '05',
    icon: <svg viewBox="0 0 16 16" fill="none" className="w-4 h-4" stroke="currentColor" strokeWidth="1.5">
      <path d="M2 11 Q8 4 14 11"/><line x1="8" y1="4" x2="8" y2="2"/><line x1="1" y1="13" x2="15" y2="13"/>
      <line x1="3.5" y1="6" x2="2.5" y2="4.5"/><line x1="12.5" y1="6" x2="13.5" y2="4.5"/>
    </svg>,
  },
  {
    path: '/friction-heatmap', label: 'Friction Heatmap', tag: '06',
    icon: <svg viewBox="0 0 16 16" fill="none" className="w-4 h-4" stroke="currentColor" strokeWidth="1.5">
      <polyline points="8,1 5,7 8,7 5,15"/><line x1="3" y1="5" x2="6.5" y2="5"/><line x1="9.5" y1="9" x2="13" y2="9"/>
    </svg>,
  },
  {
    path: '/workflow-discovery', label: 'Workflow', tag: '07',
    icon: <svg viewBox="0 0 16 16" fill="none" className="w-4 h-4" stroke="currentColor" strokeWidth="1.5">
      <circle cx="2.5" cy="8" r="1.5"/><circle cx="13.5" cy="3.5" r="1.5"/><circle cx="13.5" cy="12.5" r="1.5"/>
      <line x1="4" y1="8" x2="12" y2="3.5"/><line x1="4" y1="8" x2="12" y2="12.5"/>
    </svg>,
  },
  {
    path: '/adoption-velocity', label: 'Adoption', tag: '08',
    icon: <svg viewBox="0 0 16 16" fill="none" className="w-4 h-4" stroke="currentColor" strokeWidth="1.5">
      <path d="M1 13 C4 13 4 4 7 4 C10 4 10 10 13 7 L15 5"/>
      <polyline points="12,3 15,5 13,8"/>
    </svg>,
  },
  {
    path: '/peak-pressure', label: 'Peak Pressure', tag: '09',
    icon: <svg viewBox="0 0 16 16" fill="none" className="w-4 h-4" stroke="currentColor" strokeWidth="1.5">
      <path d="M8 14 A6 6 0 1 1 14 8"/><polyline points="8,5 8,8 10,10"/>
      <circle cx="8" cy="8" r="0.5" fill="currentColor"/>
    </svg>,
  },
  {
    path: '/anomaly-guard', label: 'Anomaly Guard', tag: '10',
    icon: <svg viewBox="0 0 16 16" fill="none" className="w-4 h-4" stroke="currentColor" strokeWidth="1.5">
      <path d="M8 1.5 L14.5 13 H1.5 Z"/><line x1="8" y1="6" x2="8" y2="9.5"/>
      <circle cx="8" cy="11.5" r="0.6" fill="currentColor" stroke="none"/>
    </svg>,
  },
]

export default function Layout({ children }: { children: ReactNode }) {
  const [collapsed, setCollapsed] = useState(false)
  const location = useLocation()
  const current = NAV.find(n => n.path !== '/' ? location.pathname.startsWith(n.path) : location.pathname === '/')

  return (
    <div className="flex h-full">
      {/* Sidebar */}
      <aside className={`
        flex flex-col flex-shrink-0 bg-surface border-r border-border
        transition-all duration-200 ease-out
        ${collapsed ? 'w-14' : 'w-52'}
      `}>
        {/* Logo */}
        <div className={`flex items-center gap-2.5 px-4 h-14 border-b border-border flex-shrink-0 ${collapsed ? 'justify-center px-2' : ''}`}>
          <div className="w-6 h-6 rounded bg-accent/10 border border-accent/30 flex items-center justify-center flex-shrink-0">
            <svg viewBox="0 0 12 12" fill="none" className="w-3 h-3" stroke="#22d3ee" strokeWidth="1.5">
              <polyline points="1,9 4,5 7,7 11,2"/>
            </svg>
          </div>
          {!collapsed && (
            <div className="min-w-0">
              <div className="text-sm font-semibold text-primary leading-none">STAR</div>
              <div className="text-[10px] text-secondary mt-0.5 leading-none">Perf Monitor</div>
            </div>
          )}
          {!collapsed && (
            <button onClick={() => setCollapsed(true)} className="ml-auto text-muted hover:text-secondary transition-colors">
              <svg viewBox="0 0 16 16" fill="none" className="w-4 h-4" stroke="currentColor" strokeWidth="1.5">
                <path d="M10 3 L5 8 L10 13"/>
              </svg>
            </button>
          )}
        </div>

        {/* Collapse toggle when collapsed */}
        {collapsed && (
          <button onClick={() => setCollapsed(false)} className="flex items-center justify-center h-8 text-muted hover:text-secondary transition-colors mx-2 mt-2 rounded border border-border">
            <svg viewBox="0 0 16 16" fill="none" className="w-3.5 h-3.5" stroke="currentColor" strokeWidth="1.5">
              <path d="M6 3 L11 8 L6 13"/>
            </svg>
          </button>
        )}

        {/* Nav */}
        <nav className="flex-1 overflow-y-auto py-3 px-2">
          {NAV.map((item, idx) => (
            <div key={item.path}>
              {/* Divider before UC-01 */}
              {idx === 2 && !collapsed && (
                <div className="my-2 border-t border-border/50" />
              )}
              {idx === 2 && collapsed && <div className="my-1 border-t border-border/50 mx-1" />}
            <NavLink
              to={item.path}
              end={item.path === '/'}
              className={({ isActive }) => `
                flex items-center gap-2.5 px-2 py-2 rounded text-sm mb-0.5
                transition-colors duration-100 group relative
                ${isActive
                  ? 'bg-accent/10 text-accent border border-accent/20'
                  : 'text-secondary hover:text-primary hover:bg-raised border border-transparent'}
                ${collapsed ? 'justify-center' : ''}
              `}
              title={collapsed ? item.label : undefined}
            >
              <span className="flex-shrink-0">{item.icon}</span>
              {!collapsed && (
                <>
                  <span className="flex-1 truncate">{item.label}</span>
                  {item.tag && (
                    <span className="text-[10px] font-mono text-muted group-hover:text-secondary transition-colors">
                      {item.tag}
                    </span>
                  )}
                </>
              )}
            </NavLink>
            </div>
          ))}
        </nav>

        {/* Footer */}
        {!collapsed && (
          <div className="px-4 py-3 border-t border-border flex-shrink-0">
            <div className="text-[10px] text-muted font-mono">v1.0</div>
          </div>
        )}
      </aside>

      {/* Main */}
      <div className="flex-1 flex flex-col min-w-0 overflow-hidden">
        {/* Top bar */}
        <header className="h-14 border-b border-border flex items-center px-6 gap-4 flex-shrink-0 bg-surface/60 backdrop-blur-sm">
          <div>
            <h1 className="text-sm font-semibold text-primary">{current?.label ?? 'Overview'}</h1>
          </div>
          <div className="ml-auto flex items-center gap-2">
            <a
              href="http://localhost:8000/docs"
              target="_blank"
              rel="noopener noreferrer"
              className="btn-ghost text-xs"
            >
              <svg viewBox="0 0 16 16" fill="none" className="w-3.5 h-3.5" stroke="currentColor" strokeWidth="1.5">
                <rect x="2" y="1.5" width="12" height="13" rx="1.5"/>
                <line x1="5" y1="5.5" x2="11" y2="5.5"/>
                <line x1="5" y1="8" x2="11" y2="8"/>
                <line x1="5" y1="10.5" x2="9" y2="10.5"/>
              </svg>
              API Docs
            </a>
          </div>
        </header>

        {/* Page content */}
        <main className="flex-1 overflow-y-auto p-6 animate-fade-in">
          {children}
        </main>
      </div>
    </div>
  )
}
