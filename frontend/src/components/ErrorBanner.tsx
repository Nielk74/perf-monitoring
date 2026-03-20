export default function ErrorBanner({ error }: { error: unknown }) {
  const msg = error instanceof Error ? error.message : String(error)
  return (
    <div className="flex items-start gap-3 px-4 py-3 rounded-lg border border-down/20 bg-down/5 text-sm text-down animate-fade-in">
      <svg viewBox="0 0 16 16" fill="none" className="w-4 h-4 flex-shrink-0 mt-0.5" stroke="currentColor" strokeWidth="1.5">
        <path d="M8 1.5 L14.5 13 H1.5 Z"/>
        <line x1="8" y1="6" x2="8" y2="9.5"/>
        <circle cx="8" cy="11.5" r="0.6" fill="currentColor" stroke="none"/>
      </svg>
      <span className="text-down/80">{msg}</span>
    </div>
  )
}
