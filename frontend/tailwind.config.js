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
        canvas:  '#0a0b0e',
        surface: '#111318',
        raised:  '#181c23',
        border:  '#1f2330',
        primary:   '#e8eaf0',
        secondary: '#6b7280',
        muted:     '#2d3244',
        accent: {
          DEFAULT: '#22d3ee',
          dim:     '#0e7490',
          glow:    'rgba(34,211,238,0.10)',
        },
        up:   '#34d399',
        down: '#f87171',
        warn: '#fbbf24',
      },
      boxShadow: {
        card: '0 1px 3px rgba(0,0,0,0.5), 0 0 0 1px rgba(255,255,255,0.04)',
        glow: '0 0 24px rgba(34,211,238,0.12)',
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
