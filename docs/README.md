# Performance Monitoring Platform — Technical Plan

## Project Purpose

A self-contained analytics platform that ingests Oracle audit traces and Git commit history into DuckDB, then serves a modern web dashboard for proactive performance management, product prioritization, and operational risk monitoring.

All data is mocked for development. The real Oracle source (`STAR.STAR_ACTION_AUDIT`) and application Git repositories are plugged in at deployment.

---

## Table of Contents

### Architecture & Infrastructure
| # | Document | Description |
|---|----------|-------------|
| 01 | [Architecture Overview](./01-architecture.md) | System diagram, tech stack, component boundaries |
| 02 | [DuckDB Schema](./02-duckdb-schema.md) | All tables, indexes, and derived views |

### Data Ingestion
| # | Document | Description |
|---|----------|-------------|
| 03 | [Oracle Importer](./03-ingestion-oracle.md) | Pull audit events from Oracle into DuckDB |
| 04 | [Git Importer](./04-ingestion-git.md) | Extract commits and file-change metadata |

### Backend & Frontend
| # | Document | Description |
|---|----------|-------------|
| 05 | [Analytics Engine](./05-analytics-engine.md) | Python analytics modules, query patterns |
| 06 | [Web API](./06-web-api.md) | FastAPI server, all endpoints |
| 07 | [Frontend](./07-frontend.md) | React + ECharts dashboard structure |

### Development
| # | Document | Description |
|---|----------|-------------|
| 08 | [Mock Data Generator](./08-mock-data.md) | Realistic synthetic data covering all use cases |

### Use Cases
| # | Document | Pillar | Description |
|---|----------|--------|-------------|
| UC-01 | [Silent Degrader](./use-cases/01-silent-degrader.md) | Engineering | Detect features getting 5% slower per week |
| UC-02 | [Blast Radius](./use-cases/02-blast-radius.md) | Engineering | Users/workstations impacted by a bad commit |
| UC-03 | [Environmental Fingerprint](./use-cases/03-environmental-fingerprint.md) | Engineering | Isolate hardware/network vs code regressions |
| UC-04 | [Impersonation Audit](./use-cases/04-impersonation-audit.md) | Engineering | Validate support interventions resolve friction |
| UC-05 | [Feature Sunset](./use-cases/05-feature-sunset.md) | Product | Identify zombie features with zero engagement |
| UC-06 | [Friction Heatmap](./use-cases/06-friction-heatmap.md) | Product | Rank features by cumulative wait time imposed |
| UC-07 | [Workflow Discovery](./use-cases/07-workflow-discovery.md) | Product | Map action sequences + Gantt session timeline |
| UC-08 | [Adoption Velocity](./use-cases/08-adoption-velocity.md) | Product | Measure new feature pickup rate post-release |
| UC-09 | [Peak Pressure](./use-cases/09-peak-pressure.md) | Operations | Forecast max-stress periods by time/zone/date |
| UC-10 | [Anomaly Guard](./use-cases/10-anomaly-guard.md) | Operations | Z-score alerting on behavior deviations |

---

## Repository Layout

```
perf-monitoring/
├── docs/                        # This technical plan
│   ├── README.md
│   ├── 01-architecture.md
│   ├── 02-duckdb-schema.md
│   ├── 03-ingestion-oracle.md
│   ├── 04-ingestion-git.md
│   ├── 05-analytics-engine.md
│   ├── 06-web-api.md
│   ├── 07-frontend.md
│   ├── 08-mock-data.md
│   └── use-cases/
│       ├── 01-silent-degrader.md
│       ├── 02-blast-radius.md
│       ├── 03-environmental-fingerprint.md
│       ├── 04-impersonation-audit.md
│       ├── 05-feature-sunset.md
│       ├── 06-friction-heatmap.md
│       ├── 07-workflow-discovery.md
│       ├── 08-adoption-velocity.md
│       ├── 09-peak-pressure.md
│       └── 10-anomaly-guard.md
├── src/
│   ├── ingestion/               # Oracle + Git importers, scheduler
│   ├── db/                      # DuckDB connection + migrations
│   ├── analytics/               # One module per use case
│   ├── api/                     # FastAPI app + route handlers
│   └── mock/                    # Synthetic data generator
├── frontend/                    # React + ECharts SPA
├── data/                        # perf_monitor.duckdb (gitignored)
├── config/
│   └── settings.yaml
└── requirements.txt
```
