import ReactECharts from 'echarts-for-react'

// eslint-disable-next-line @typescript-eslint/no-explicit-any
type AnyOption = Record<string, any>

interface ChartProps {
  option: AnyOption
  height?: number | string
  className?: string
  onEvents?: Record<string, (params: unknown) => void>
}

export default function Chart({ option, height = 320, className = '', onEvents }: ChartProps) {
  return (
    <ReactECharts
      option={option}
      theme="star"
      style={{ height: typeof height === 'number' ? `${height}px` : height, width: '100%' }}
      className={className}
      opts={{ renderer: 'canvas', locale: 'EN' }}
      onEvents={onEvents}
      notMerge
    />
  )
}
