import { StrictMode } from 'react'
import { createRoot } from 'react-dom/client'
import { QueryClient, QueryClientProvider } from '@tanstack/react-query'
import { BrowserRouter } from 'react-router-dom'
import * as echarts from 'echarts'
import './index.css'
import App from './App'

// Register ECharts light theme
echarts.registerTheme('star', {
  backgroundColor: 'transparent',
  color: ['#6366f1', '#22c55e', '#f97316', '#8b5cf6', '#ec4899', '#f59e0b', '#3b82f6', '#0ea5e9'],
  textStyle: { color: '#737373', fontFamily: 'Inter, system-ui, sans-serif', fontSize: 12 },
  title: { textStyle: { color: '#1a1a1a', fontWeight: '500' } },
  legend: { textStyle: { color: '#737373' }, pageTextStyle: { color: '#737373' } },
  tooltip: {
    backgroundColor: '#ffffff',
    borderColor: '#e5e5e5',
    borderWidth: 1,
    textStyle: { color: '#1a1a1a', fontSize: 12 },
    extraCssText: 'box-shadow: 0 4px 20px rgba(0,0,0,0.08); border-radius: 8px; padding: 10px 14px;',
  },
  axisPointer: { lineStyle: { color: '#e5e5e5' }, crossStyle: { color: '#e5e5e5' } },
  categoryAxis: {
    axisLine:  { lineStyle: { color: '#e5e5e5' } },
    axisTick:  { show: false },
    axisLabel: { color: '#737373', fontSize: 11 },
    splitLine: { lineStyle: { color: '#f5f5f5', type: 'dashed' } },
    splitArea: { show: false },
  },
  valueAxis: {
    axisLine:  { show: false },
    axisTick:  { show: false },
    axisLabel: { color: '#737373', fontSize: 11 },
    splitLine: { lineStyle: { color: '#f5f5f5', type: 'dashed' } },
    splitArea: { show: false },
  },
  timeAxis: {
    axisLine:  { lineStyle: { color: '#e5e5e5' } },
    axisTick:  { show: false },
    axisLabel: { color: '#737373', fontSize: 11 },
    splitLine: { lineStyle: { color: '#f5f5f5', type: 'dashed' } },
  },
  line: { smooth: false, symbolSize: 4 },
  bar:  { barMaxWidth: 40, itemStyle: { borderRadius: [3, 3, 0, 0] } },
  sankey: {
    itemStyle: { borderWidth: 0 },
    label: { color: '#737373', fontSize: 11 },
    lineStyle: { opacity: 0.3 },
  },
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
