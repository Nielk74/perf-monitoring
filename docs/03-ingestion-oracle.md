# 03 — Oracle Importer

## Responsibility

Pull rows from `STAR.STAR_ACTION_AUDIT` into the local `audit_events` DuckDB table. Runs on a nightly cron. Supports both a full initial load and incremental updates.

---

## Module: `src/ingestion/oracle_importer.py`

### Dependencies
```
python-oracledb>=2.0
duckdb>=1.0
pyyaml
```

### High-Level Flow

```
1. Open DuckDB connection
2. Read high-water mark: MAX(mod_dt) FROM audit_events
3. Open Oracle connection (thin mode — no client install)
4. Stream rows WHERE MOD_DT > high_water_mark AND MOD_DT > NOW() - 12 months
5. Batch insert into DuckDB (10,000 rows/batch)
6. Commit. Log row count and duration.
```

### Oracle Query

```sql
SELECT
    SUPP_USER,
    ASMD_USER,
    WORKSTATION,
    MOD_DT,
    FEATURE_TYPE,
    FEATURE,
    DETAIL,
    DURATION_MS
FROM STAR.STAR_ACTION_AUDIT
WHERE MOD_DT > :high_water_mark
  AND MOD_DT > SYSTIMESTAMP - INTERVAL '12' MONTH
ORDER BY MOD_DT ASC
```

Parameters:
- `:high_water_mark` — the MAX(mod_dt) from `audit_events`, or `SYSTIMESTAMP - INTERVAL '12' MONTH` for the first run.

### Deduplication

On re-run (e.g., after a crash), rows may be re-fetched. Deduplication uses a surrogate key hash before insert:

```python
import hashlib

def row_key(row) -> int:
    sig = f"{row.supp_user}|{row.asmd_user}|{row.workstation}|{row.mod_dt}|{row.feature}|{row.duration_ms}"
    return int(hashlib.md5(sig.encode()).hexdigest(), 16) % (2**63)
```

Insert uses `INSERT OR IGNORE INTO audit_events ...` with this `id`.

### Batch Insert Pattern

```python
BATCH_SIZE = 10_000

def import_batch(duckdb_conn, rows: list[dict]):
    duckdb_conn.executemany("""
        INSERT OR IGNORE INTO audit_events
            (id, supp_user, asmd_user, workstation, mod_dt,
             feature_type, feature, detail, duration_ms)
        VALUES (?, ?, ?, ?, ?, ?, ?, ?, ?)
    """, [
        (row_key(r), r['supp_user'], r['asmd_user'], r['workstation'],
         r['mod_dt'], r['feature_type'], r['feature'], r['detail'], r['duration_ms'])
        for r in rows
    ])
```

### Connection (Thin Mode)

```python
import oracledb

conn = oracledb.connect(
    user=config.oracle.user,
    password=config.oracle.password,
    dsn=f"{config.oracle.host}:{config.oracle.port}/{config.oracle.service_name}"
)
```

No Oracle Instant Client installation needed in thin mode.

---

## Incremental State

The high-water mark is stored directly in DuckDB — no separate state file needed:

```sql
SELECT MAX(mod_dt) FROM audit_events
```

If `audit_events` is empty, the importer defaults to `NOW() - 12 months`.

---

## Error Handling

| Error | Behavior |
|-------|----------|
| Oracle connection failure | Log error, raise — scheduler will retry next run |
| Row parse failure | Log warning + skip row (don't abort the batch) |
| DuckDB write failure | Rollback batch, raise |
| Partial batch on crash | Re-run safely — deduplication handles re-inserts |

---

## CLI Usage

```bash
# Full run (incremental from high-water mark)
python -m src.ingestion.oracle_importer

# Force full reload (last N days)
python -m src.ingestion.oracle_importer --days 30

# Dry-run: count rows that would be imported, no write
python -m src.ingestion.oracle_importer --dry-run
```

---

## Mock Mode

For local development without Oracle access, we ingest from a simple sqlite file with the same schema.
