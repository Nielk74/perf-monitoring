# 04 — Git Importer

## Responsibility

Walk one or more local Git repositories and extract commit metadata + file-change statistics into DuckDB (`commits`, `commit_files` tables). Enables the Blast Radius use case and commit-to-performance correlation across all use cases.

---

## Module: `src/ingestion/git_importer.py`

### Dependencies
```
gitpython>=3.1
duckdb>=1.0
```

---

## High-Level Flow

```
For each configured repo:
  1. Open repo with GitPython
  2. Walk commits (newest first), stop at max_commits or already-imported hash
  3. For each commit:
     a. Extract metadata (hash, author, date, message, tags)
     b. Extract file diff stats (added/modified/deleted, line counts)
     c. Insert into DuckDB
  4. Log imported commit count
```

---

## Data Extracted per Commit

### `commits` table fields

| Field | Source | Notes |
|-------|--------|-------|
| `commit_hash` | `commit.hexsha` | 40-char SHA |
| `repo_name` | config | friendly name for the repo |
| `author_name` | `commit.author.name` | |
| `author_email` | `commit.author.email` | |
| `committed_at` | `commit.committed_datetime` | normalized to UTC |
| `message` | `commit.message` | first 1000 chars |
| `is_merge` | `len(commit.parents) > 1` | |
| `tag` | tag map lookup | nearest tag name, if any |
| `deployed_at` | `None` initially | filled manually or via CI hook |

### `commit_files` table fields

| Field | Source |
|-------|--------|
| `commit_hash` | parent commit |
| `file_path` | diff stat key |
| `change_type` | A / M / D / R |
| `lines_added` | `diff.additions` |
| `lines_removed` | `diff.deletions` |

---

## Tag Resolution

Tags allow mapping a commit to a named release, which is the reference point for the Blast Radius and Adoption Velocity use cases.

```python
from git import Repo

def build_tag_map(repo: Repo) -> dict[str, str]:
    """Returns {commit_hash: tag_name} for tagged commits."""
    return {tag.commit.hexsha: tag.name for tag in repo.tags}
```

For commits between two tags, `nearest_tag` is the most recent tag reachable from that commit:

```python
def nearest_tag(repo: Repo, commit) -> str | None:
    try:
        return repo.git.describe("--tags", "--abbrev=0", commit.hexsha)
    except Exception:
        return None
```

---

## Incremental Import

Already-imported commits are skipped by checking existence in DuckDB:

```python
def get_known_hashes(duckdb_conn) -> set[str]:
    result = duckdb_conn.execute("SELECT commit_hash FROM commits").fetchall()
    return {row[0] for row in result}
```

Walk stops when a commit hash is already in the set, or when `max_commits` is reached.

---

## `deployed_at` Population

`deployed_at` is not in the Git data — it represents the moment a commit became live to users. Three strategies to populate it:

1. **Manual**: update DuckDB directly after a deployment
   ```sql
   UPDATE commits SET deployed_at = '2026-03-15 10:00:00' WHERE tag = 'v2.14.0';
   ```

2. **CI/CD hook**: POST to `/api/ingestion/deployment` with `{commit_hash, deployed_at}` after a deploy pipeline completes.

3. **Approximation**: use `committed_at + median_deploy_lag` as an estimate. The median lag (time from commit to production) can be inferred from known deploy events.

For the mock dataset, `deployed_at` is set to `committed_at + random(30min, 4h)`.

---

## Multi-Repo Support

All repos share the same `commits` and `commit_files` tables. `repo_name` distinguishes them. This allows:
- Blast Radius scoped to a specific repo
- Cross-repo commit correlation (e.g., shared library changes)

```yaml
git:
  repos:
    - path: /repos/star-app
      name: star-app
    - path: /repos/star-shared-lib
      name: star-shared-lib
```

---

## CLI Usage

```bash
# Import all configured repos
python -m src.ingestion.git_importer

# Import a specific repo by name
python -m src.ingestion.git_importer --repo star-app

# Limit to last N commits
python -m src.ingestion.git_importer --max-commits 500

# Dry-run
python -m src.ingestion.git_importer --dry-run
```

---

## Error Handling

| Error | Behavior |
|-------|----------|
| Repo path not found | Log error, skip repo, continue others |
| Corrupt commit object | Log warning, skip commit |
| Diff too large (>1000 files) | Store commit metadata only, skip file-level diff |

---

## Mock Mode

When no real repo is configured, the mock generator creates synthetic commits with realistic patterns:
- Commits every 1-3 days over 12 months
- Tags every 2-4 weeks (release cadence)
- File paths follow realistic patterns (`src/features/search.py`, `src/ui/tools/`)
- `deployed_at` set to commit time + 2h
