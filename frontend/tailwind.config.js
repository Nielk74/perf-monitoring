/** @type {import('tailwindcss').Config} */
export default {
  content: ['./index.html', './src/**/*.{ts,tsx}'],
  theme: {
    extend: {
      fontFamily: {
        sans: ['Inter', 'system-ui', 'sans-serif'],
        mono: ['JetBrains Mono', 'ui-monospace', 'monospace'],
      },
      colors: {
        canvas:  '#f5f5f7',
        surface: '#ffffff',
        raised:  '#fafafa',
        border:  '#e5e5e5',
        primary:   '#1a1a1a',
        secondary: '#737373',
        muted:     '#d4d4d4',
        accent: {
          DEFAULT: '#6366f1',
          dim:     '#c7d2fe',
          glow:    'rgba(99,102,241,0.08)',
        },
        up:   '#22c55e',
        down: '#ef4444',
        warn: '#f59e0b',
      },
      boxShadow: {
        card: '0 1px 2px rgba(0,0,0,0.05), 0 0 0 1px rgba(0,0,0,0.06)',
        glow: '0 0 24px rgba(99,102,241,0.12)',
      },
      animation: {
        'fade-in':  'fadeIn 0.18s ease-out',
        'slide-up': 'slideUp 0.22s ease-out',
      },
      keyframes: {
        fadeIn:  { from: { opacity: '0' }, to: { opacity: '1' } },
        slideUp: { from: { opacity: '0', transform: 'translateY(6px)' }, to: { opacity: '1', transform: 'translateY(0)' } },
      },
    },
  },
  plugins: [],
}
