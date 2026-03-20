import { StrictMode } from 'react'
import { createRoot } from 'react-dom/client'
import { QueryClient, QueryClientProvider } from '@tanstack/react-query'
import { BrowserRouter } from 'react-router-dom'
import * as echarts from 'echarts'
import './index.css'
import App from './App'

// Register ECharts dark theme
echarts.registerTheme('star', {
  backgroundColor: 'transparent',
  color: ['#22d3ee', '#818cf8', '#34d399', '#fb923c', '#a78bfa', '#f472b6', '#fbbf24', '#60a5fa'],
  textStyle: { color: '#6b7280', fontFamily: 'Inter, system-ui, sans-serif', fontSize: 12 },
  title: { textStyle: { color: '#e8eaf0', fontWeight: '500' } },
  legend: { textStyle: { color: '#6b7280' }, pageTextStyle: { color: '#6b7280' } },
  tooltip: {
    backgroundColor: '#181c23',
    borderColor: '#1f2330',
    borderWidth: 1,
    textStyle: { color: '#e8eaf0', fontSize: 12 },
    extraCssText: 'box-shadow: 0 4px 16px rgba(0,0,0,0.5); border-radius: 6px;',
  },
  axisPointer: { lineStyle: { color: '#2d3244' }, crossStyle: { color: '#2d3244' } },
  categoryAxis: {
    axisLine:  { lineStyle: { color: '#1f2330' } },
    axisTick:  { show: false },
    axisLabel: { color: '#6b7280', fontSize: 11 },
    splitLine: { lineStyle: { color: '#1f2330', type: 'dashed' } },
  },
  valueAxis: {
    axisLine:  { show: false },
    axisTick:  { show: false },
    axisLabel: { color: '#6b7280', fontSize: 11 },
    splitLine: { lineStyle: { color: '#1f2330', type: 'dashed' } },
  },
  line: { smooth: false, symbolSize: 4 },
  bar:  { barMaxWidth: 40, itemStyle: { borderRadius: [3, 3, 0, 0] } },
})

const qc = new QueryClient({
  defaultOptions: { queries: { staleTime: 30_000, retry: 1 } },
})

createRoot(document.getElementById('root')!).render(
  <StrictMode>
    <QueryClientProvider client={qc}>
      <BrowserRouter>
        <App />
      </BrowserRouter>
    </QueryClientProvider>
  </StrictMode>,
)
