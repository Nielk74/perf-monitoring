# 07 — Frontend

## Stack

| Technology | Version | Role |
|-----------|---------|------|
| React | 18 | UI framework |
| TypeScript | 5 | Type safety |
| Vite | 5 | Build tool + dev server |
| React Router | 6 | Client-side routing |
| Apache ECharts + echarts-for-react | 5 | All charts and visualizations |
| TanStack Query | 5 | API data fetching + caching |
| Tailwind CSS | 3 | Utility-first styling |

No component library (Ant Design, MUI, etc.) — keeps the bundle lean and avoids opinionated styling conflicts with ECharts.

---

## Directory Structure

```
frontend/
├── src/
│   ├── main.tsx                  # App entry point
│   ├── App.tsx                   # Router setup
│   ├── api/
│   │   └── client.ts             # Typed fetch wrappers for each API endpoint
│   ├── components/
│   │   ├── Layout.tsx            # Sidebar nav + top filter bar
│   │   ├── FilterBar.tsx         # Date range, feature type, office pickers
│   │   ├── LoadingSpinner.tsx
│   │   └── ErrorBoundary.tsx
│   └── pages/
│       ├── Home.tsx              # Summary dashboard (KPI cards)
│       ├── SilentDegrader.tsx    # UC-01
│       ├── BlastRadius.tsx       # UC-02
│       ├── EnvironmentalFingerprint.tsx  # UC-03
│       ├── ImpersonationAudit.tsx        # UC-04
│       ├── FeatureSunset.tsx     # UC-05
│       ├── FrictionHeatmap.tsx   # UC-06
│       ├── WorkflowDiscovery.tsx # UC-07
│       ├── AdoptionVelocity.tsx  # UC-08
│       ├── PeakPressure.tsx      # UC-09
│       └── AnomalyGuard.tsx      # UC-10
├── index.html
├── vite.config.ts
├── tailwind.config.ts
└── package.json
```

---

## Routing

```tsx
// App.tsx
<Routes>
  <Route path="/" element={<Home />} />
  <Route path="/silent-degrader" element={<SilentDegrader />} />
  <Route path="/blast-radius" element={<BlastRadius />} />
  <Route path="/environmental-fingerprint" element={<EnvironmentalFingerprint />} />
  <Route path="/impersonation-audit" element={<ImpersonationAudit />} />
  <Route path="/feature-sunset" element={<FeatureSunset />} />
  <Route path="/friction-heatmap" element={<FrictionHeatmap />} />
  <Route path="/workflow-discovery" element={<WorkflowDiscovery />} />
  <Route path="/adoption-velocity" element={<AdoptionVelocity />} />
  <Route path="/peak-pressure" element={<PeakPressure />} />
  <Route path="/anomaly-guard" element={<AnomalyGuard />} />
</Routes>
```

---

## API Client: `src/api/client.ts`

All endpoints are typed. The client wraps `fetch` with base URL from env:

```ts
const BASE = import.meta.env.VITE_API_URL ?? "http://localhost:8000";

async function get<T>(path: string, params?: Record<string, string>): Promise<T> {
  const url = new URL(`${BASE}${path}`);
  if (params) Object.entries(params).forEach(([k, v]) => url.searchParams.set(k, v));
  const res = await fetch(url.toString());
  if (!res.ok) throw new Error(`API error ${res.status}`);
  return res.json();
}
```

Each endpoint gets a typed function, e.g.:
```ts
export const getSilentDegrader = (weeks: number, slopeThresholdPct: number) =>
  get<SilentDegraderResult>("/api/analytics/silent-degrader", {
    weeks: String(weeks),
    slope_threshold_pct: String(slopeThresholdPct)
  });
```

---

## Visualization Map

| Page | Chart Type | ECharts Component |
|------|-----------|-------------------|
| Silent Degrader | Multi-line time series (p95 per feature) + slope annotation | `LineChart` |
| Blast Radius | Treemap (impacted users) + timeline bar | `TreemapChart` + `BarChart` |
| Environmental Fingerprint | Box plot per office | `BoxplotChart` |
| Impersonation Audit | Grouped bar (support vs normal session durations) | `BarChart` |
| Feature Sunset | Horizontal bar (last-used date) + zero-usage list | `BarChart` |
| Friction Heatmap | Horizontal ranked bar (total_wait_ms) | `BarChart` |
| Workflow Discovery | Sankey diagram (transitions) + Gantt timeline | `SankeyChart` + custom `CustomChart` |
| Adoption Velocity | Area chart (cumulative users over time) | `LineChart` |
| Peak Pressure | Heatmap (hour × day-of-week) + calendar heatmap | `HeatmapChart` |
| Anomaly Guard | Scatter (z-score) + alert table | `ScatterChart` |

---

## Shared Filter Bar

`FilterBar.tsx` renders at the top of every use-case page. It manages:
- `startDate` / `endDate` (date range picker)
- `featureType` (dropdown — populated from `/api/metadata/features`)
- `office` (dropdown — populated from `/api/metadata/offices`)

Filters are stored in React state and passed as props to each page. Changing a filter re-triggers the TanStack Query fetch.

---

## Gantt Timeline (Workflow Discovery)

The Gantt view renders per-user session timelines. ECharts `custom` series is used since there is no native Gantt chart:

```ts
// Each item: { user, feature, start, end }
series: [{
  type: 'custom',
  renderItem: (params, api) => ({
    type: 'rect',
    shape: {
      x: api.coord([api.value(2), api.value(0)])[0],
      y: api.coord([api.value(2), api.value(0)])[1] - barHeight / 2,
      width: (api.coord([api.value(3), 0])[0] - api.coord([api.value(2), 0])[0]),
      height: barHeight
    },
    style: api.style()
  })
}]
```

Each row is a user session. Each bar is a feature action, colored by `feature_type`.

---

## Dev Setup

```bash
cd frontend
npm install
npm run dev        # starts on :5173, proxies /api/* to :8000
```

`vite.config.ts` proxy:
```ts
server: {
  proxy: {
    "/api": "http://localhost:8000"
  }
}
```

---

## Build for Production

```bash
npm run build      # outputs to frontend/dist/
```

The FastAPI server can serve `frontend/dist/` as static files, or Nginx can serve it directly with a reverse proxy to the API.
