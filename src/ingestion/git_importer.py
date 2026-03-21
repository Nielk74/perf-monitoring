"""
src/ingestion/git_importer.py

Walks configured Git repositories and imports commit metadata + file-change
stats into DuckDB (commits, commit_files tables).

Supports incremental import: stops walking when a commit hash is already
in DuckDB. deployed_at is set to committed_at + 2h for tagged commits;
NULL otherwise (can be populated later via CI hook or manual SQL).

Usage:
    python -m src.ingestion.git_importer                    # all repos from settings.yaml
    python -m src.ingestion.git_importer --repo star-app    # specific repo
    python -m src.ingestion.git_importer --max-commits 500
    python -m src.ingestion.git_importer --dry-run
"""

import argparse
import csv
import os
import re
import subprocess
import sys
import tempfile
import time
from concurrent.futures import ThreadPoolExecutor
from datetime import datetime, timedelta, timezone
from pathlib import Path

import git
import yaml

from src.db.connection import get_connection

SETTINGS_PATH  = "config/settings.yaml"
MAX_DIFF_FILES = 1000   # skip file-level diff if commit touches more than this

_TAG_RE = re.compile(r"(?:^|,\s*)tag:\s*([^,)]+)")
# git format-string hex escapes (safe on all platforms)
_FMT = "%x1f".join(["%H", "%P", "%aN", "%aE", "%ct", "%D", "%B"]) + "%x1e"


# ---------------------------------------------------------------------------
# Config
# ---------------------------------------------------------------------------

def load_repos(settings_path: str, repo_filter: str | None) -> list[dict]:
    with open(settings_path, encoding="utf-8") as f:
        cfg = yaml.safe_load(f)
    repos = cfg.get("git", {}).get("repos", [])
    if repo_filter:
        repos = [r for r in repos if r["name"] == repo_filter]
    return repos


# ---------------------------------------------------------------------------
# Tag resolution
# ---------------------------------------------------------------------------

def build_tag_map(repo: git.Repo) -> dict[str, str]:
    """Returns {commit_hexsha: tag_name} for all tagged commits."""
    result = {}
    for tag in repo.tags:
        try:
            # Annotated tags point to a tag object, not directly to a commit
            commit = tag.commit
            result[commit.hexsha] = tag.name
        except Exception:
            pass
    return result


def nearest_tag(repo: git.Repo, commit: git.Commit) -> str | None:
    try:
        return repo.git.describe("--tags", "--abbrev=0", commit.hexsha)
    except git.GitCommandError:
        return None


# ---------------------------------------------------------------------------
# deployed_at heuristic
# ---------------------------------------------------------------------------

def estimate_deployed_at(commit: git.Commit, tag_map: dict[str, str]) -> datetime | None:
    """
    For tagged commits: deployed_at = committed_at + 2h (simulates CI pipeline lag).
    For untagged commits: None (must be set manually or via /api/ingestion/deployment).
    """
    if commit.hexsha in tag_map:
        committed = datetime.fromtimestamp(commit.committed_date, tz=timezone.utc)
        return committed + timedelta(hours=2)
    return None


# ---------------------------------------------------------------------------
# Known hashes (incremental)
# ---------------------------------------------------------------------------

def get_known_hashes(conn) -> set[str]:
    return {row[0] for row in conn.execute("SELECT commit_hash FROM commits").fetchall()}


# ---------------------------------------------------------------------------
# Bulk git log helpers  (O(1) subprocess calls regardless of commit count)
# ---------------------------------------------------------------------------

def _git_out(repo_path: str, args: list[str]) -> str:
    return subprocess.check_output(
        ["git", "-C", repo_path] + args,
        stderr=subprocess.DEVNULL,
        encoding="utf-8", errors="replace",
    )


def _bulk_metadata(repo_path: str, max_commits: int | None) -> list[dict]:
    """One git-log call → all commit metadata (standalone, not used when
    name-status is also needed — see _bulk_namestatus_with_meta)."""
    cmd = ["log", f"--format={_FMT}"]
    if max_commits:
        cmd += ["-n", str(max_commits)]
    out = _git_out(repo_path, cmd)
    return _parse_metadata(out)


def _parse_metadata(out: str) -> list[dict]:
    records = []
    for rec in out.split("\x1e"):
        rec = rec.strip()
        if not rec:
            continue
        parts = rec.split("\x1f", 6)
        if len(parts) < 7:
            continue
        refs    = parts[5]
        tag_m   = _TAG_RE.search(refs)
        tag     = tag_m.group(1).strip() if tag_m else None
        ts      = int(parts[4]) if parts[4].lstrip("-").isdigit() else 0
        parents = parts[1].split() if parts[1].strip() else []
        records.append({
            "commit_hash":  parts[0].strip(),
            "parents":      parents,
            "author_name":  parts[2],
            "author_email": parts[3],
            "committed_ts": ts,
            "message":      parts[6][:1000],
            "tag":          tag,
        })
    return records


def _bulk_namestatus_with_meta(
    repo_path: str, max_commits: int | None
) -> tuple[list[dict], dict[str, dict[str, str]]]:
    """Single streaming git-log call: commit metadata + name-status."""
    meta_fmt = "%x1f".join(["%H", "%P", "%aN", "%aE", "%ct", "%D", "%s"])
    cmd = ["git", "-C", repo_path,
           "log", f"--format=META {meta_fmt}", "--name-status"]
    if max_commits:
        cmd += ["-n", str(max_commits)]

    meta_records: list[dict] = []
    namestatus: dict[str, dict[str, str]] = {}
    cur_hash: str | None = None

    with subprocess.Popen(cmd, stdout=subprocess.PIPE, stderr=subprocess.DEVNULL,
                          encoding="utf-8", errors="replace", bufsize=1 << 20) as proc:
        for line in proc.stdout:
            line = line.rstrip("\n")
            if line.startswith("META "):
                parts = line[5:].split("\x1f", 6)
                if len(parts) < 7:
                    continue
                refs     = parts[5]
                tag_m    = _TAG_RE.search(refs)
                tag      = tag_m.group(1).strip() if tag_m else None
                ts       = int(parts[4]) if parts[4].lstrip("-").isdigit() else 0
                parents  = parts[1].split() if parts[1].strip() else []
                cur_hash = parts[0].strip()
                meta_records.append({
                    "commit_hash":  cur_hash,
                    "parents":      parents,
                    "author_name":  parts[2],
                    "author_email": parts[3],
                    "committed_ts": ts,
                    "message":      parts[6][:1000],
                    "tag":          tag,
                })
                namestatus[cur_hash] = {}
            elif cur_hash and line.strip():
                fp = line.split("\t")
                if len(fp) >= 2:
                    namestatus[cur_hash][fp[-1].strip()] = fp[0][0].upper()

    return meta_records, namestatus


_NUMSTAT_WORKERS = 4   # parallel git-log --numstat processes


def _numstat_chunk(repo_path: str, n: int, skip: int) -> dict[str, list[tuple]]:
    """Run git log --numstat for a slice of commits (used by parallel workers)."""
    cmd = ["git", "-C", repo_path, "log",
           "--format=COMMIT %H", "--numstat",
           "-n", str(n), "--skip", str(skip)]
    result: dict[str, list[tuple]] = {}
    cur = None
    with subprocess.Popen(cmd, stdout=subprocess.PIPE, stderr=subprocess.DEVNULL,
                          encoding="utf-8", errors="replace", bufsize=1 << 20) as proc:
        for line in proc.stdout:
            line = line.rstrip("\n")
            if line.startswith("COMMIT "):
                cur = line[7:]
                result.setdefault(cur, [])
            elif cur and "\t" in line:
                parts = line.split("\t", 2)
                if len(parts) == 3:
                    try:
                        added   = int(parts[0]) if parts[0] != "-" else 0
                        removed = int(parts[1]) if parts[1] != "-" else 0
                        result[cur].append((parts[2], added, removed))
                    except ValueError:
                        pass
    return result


def _bulk_numstat(repo_path: str, max_commits: int | None) -> dict[str, list[tuple]]:
    """Parallel git-log --numstat across N worker chunks → merged result."""
    total = max_commits or int(subprocess.check_output(
        ["git", "-C", repo_path, "rev-list", "--count", "HEAD"],
        stderr=subprocess.DEVNULL,
        encoding="utf-8", errors="replace",
    ).strip())
    chunk = max(1, (total + _NUMSTAT_WORKERS - 1) // _NUMSTAT_WORKERS)

    with ThreadPoolExecutor(max_workers=_NUMSTAT_WORKERS) as ex:
        futures = [
            ex.submit(_numstat_chunk, repo_path, chunk, skip)
            for skip in range(0, total, chunk)
        ]
        merged: dict[str, list[tuple]] = {}
        for f in futures:
            merged.update(f.result())
    return merged


def _bulk_namestatus(repo_path: str, max_commits: int | None) -> dict[str, dict[str, str]]:
    """One git-log --name-status call → {hash: {path: change_type}}."""
    cmd = ["log", "--format=COMMIT %H", "--name-status"]
    if max_commits:
        cmd += ["-n", str(max_commits)]
    out = _git_out(repo_path, cmd)
    result: dict[str, dict[str, str]] = {}
    cur = None
    for line in out.splitlines():
        if line.startswith("COMMIT "):
            cur = line[7:].strip()
            result.setdefault(cur, {})
        elif cur and line.strip():
            parts = line.split("\t")
            if len(parts) >= 2:
                ctype = parts[0][0].upper()          # A M D R C
                path  = parts[-1].strip()            # for renames: new path
                result[cur][path] = ctype
    return result


# ---------------------------------------------------------------------------
# DuckDB bulk insert helpers
# ---------------------------------------------------------------------------

_CSV_COL_TYPES = {
    "commits": (
        "'commit_hash': 'VARCHAR', 'repo_name': 'VARCHAR', "
        "'author_name': 'VARCHAR', 'author_email': 'VARCHAR', "
        "'committed_at': 'VARCHAR', 'message': 'VARCHAR', "
        "'is_merge': 'BOOLEAN', 'tag': 'VARCHAR', 'deployed_at': 'VARCHAR'"
    ),
    "commit_files": (
        "'commit_hash': 'VARCHAR', 'file_path': 'VARCHAR', "
        "'change_type': 'VARCHAR', 'lines_added': 'INTEGER', "
        "'lines_removed': 'INTEGER'"
    ),
}


def _csv_col_types(table: str) -> str:
    return _CSV_COL_TYPES[table]


def _write_csv_and_get_path(rows: list) -> str:
    """Write rows to a temp CSV file and return the path."""
    with tempfile.NamedTemporaryFile(
        mode="w", suffix=".csv", delete=False, newline="", encoding="utf-8"
    ) as f:
        csv.writer(f).writerows(rows)
        return f.name


# ---------------------------------------------------------------------------
# Per-repo import  (bulk-optimised)
# ---------------------------------------------------------------------------

def import_repo(
    repo_cfg: dict,
    conn,
    max_commits: int | None,
    dry_run: bool,
) -> dict:
    repo_path = repo_cfg["path"]
    repo_name = repo_cfg["name"]

    if not Path(repo_path).exists():
        print(f"  [{repo_name}] path not found: {repo_path} — skipping")
        return {"repo": repo_name, "imported": 0, "skipped": 0, "error": "path not found"}

    if not (Path(repo_path) / ".git").exists() and not Path(repo_path).joinpath("HEAD").exists():
        print(f"  [{repo_name}] not a git repository: {repo_path} — skipping")
        return {"repo": repo_name, "imported": 0, "skipped": 0, "error": "not a git repo"}

    known = get_known_hashes(conn)
    t0 = time.monotonic()

    print(f"  [{repo_name}] reading git log …", end="", flush=True)

    # ── 2 parallel git calls + pipeline commit CSV write ─────────────────
    # name-status+meta finishes first (~0.5s); we immediately build commit_rows
    # and write their CSV while numstat is still running (~1.9s).
    with ThreadPoolExecutor(max_workers=2) as ex:
        f_meta = ex.submit(_bulk_namestatus_with_meta, repo_path, max_commits)
        f_num  = ex.submit(_bulk_numstat,              repo_path, max_commits)

        meta, namestatus = f_meta.result()  # fast — available first

        if dry_run:
            numstat = f_num.result()
            new = sum(1 for m in meta if m["commit_hash"] not in known)
            return {"repo": repo_name, "imported": new, "skipped": len(meta) - new}

        # Build and write commit CSV while numstat is still running
        commit_rows = []
        for m in meta:
            h = m["commit_hash"]
            if h in known:
                continue
            committed_at = datetime.fromtimestamp(m["committed_ts"], tz=timezone.utc)
            tag          = m["tag"]
            deployed_at  = (committed_at + timedelta(hours=2)).isoformat() if tag else None
            commit_rows.append((
                h, repo_name,
                m["author_name"], m["author_email"],
                committed_at.isoformat(), m["message"],
                len(m["parents"]) > 1, tag, deployed_at,
            ))

        # Pipeline: submit commit CSV write as a background thread
        f_commit_csv = ex.submit(_write_csv_and_get_path, commit_rows)

        # Now wait for numstat (the slow one)
        numstat = f_num.result()

        commit_tmp = f_commit_csv.result()

    skipped = len(meta) - len(commit_rows)
    print(f" {len(meta)} commits ({skipped} already known)"
          f" [{time.monotonic()-t0:.1f}s]")

    # build file rows (fast, ~0.07s)
    print(f"  [{repo_name}] building file rows …", end="", flush=True)
    t1 = time.monotonic()
    known_new = {r[0] for r in commit_rows}
    file_rows = []
    for h in known_new:
        ns_map    = namestatus.get(h, {})
        stat_list = numstat.get(h, [])
        if len(stat_list) <= MAX_DIFF_FILES:
            for path, added, removed in stat_list:
                file_rows.append((h, path, ns_map.get(path, "M"), added, removed))
    print(f" {len(file_rows):,} file rows [{time.monotonic()-t1:.1f}s]")

    # ── bulk insert via CSV COPY ──────────────────────────────────────────
    print(f"  [{repo_name}] inserting into DuckDB …", end="", flush=True)
    t2 = time.monotonic()

    def _csv_copy_load(tmp: str, table: str, ignore: bool) -> None:
        escaped  = tmp.replace("\\", "/")
        modifier = "OR IGNORE " if ignore else ""
        try:
            conn.execute(
                f"INSERT {modifier}INTO {table} "
                f"SELECT * FROM read_csv('{escaped}', header=false, "
                f"encoding='utf-8', columns={{{_csv_col_types(table)}}})"
            )
        finally:
            os.unlink(tmp)

    # commits CSV was already written during git numstat (pipelined)
    _csv_copy_load(commit_tmp, "commits", ignore=True)

    # file rows: write + load
    file_tmp = _write_csv_and_get_path(file_rows)
    _csv_copy_load(file_tmp, "commit_files", ignore=False)

    imported = len(commit_rows)
    skipped  = len(meta) - imported
    print(f" done [{time.monotonic()-t2:.1f}s]  —  "
          f"total {time.monotonic()-t0:.1f}s")
    return {"repo": repo_name, "imported": imported, "skipped": skipped}


# ---------------------------------------------------------------------------
# Entry point
# ---------------------------------------------------------------------------

if __name__ == "__main__":
    parser = argparse.ArgumentParser(description="Import git commits into DuckDB")
    parser.add_argument("--repo",        default=None,  help="Import only this repo name")
    parser.add_argument("--max-commits", type=int, default=None)
    parser.add_argument("--dry-run",     action="store_true")
    parser.add_argument("--settings",    default=SETTINGS_PATH)
    parser.add_argument("--db",          default=None)
    args = parser.parse_args()

    repos = load_repos(args.settings, args.repo)
    if not repos:
        print("No repos configured (check config/settings.yaml).")
        sys.exit(1)

    conn = get_connection(args.db) if args.db else get_connection()
    t0   = time.monotonic()

    for repo_cfg in repos:
        print(f"Importing repo: {repo_cfg['name']} ({repo_cfg['path']})")
        result = import_repo(repo_cfg, conn, args.max_commits, args.dry_run)
        label  = "would import" if args.dry_run else "imported"
        print(f"  {label}: {result['imported']}  skipped: {result['skipped']}")

    elapsed = time.monotonic() - t0
    print(f"\nDone in {elapsed:.1f}s")

    # Show release commits (those with deployed_at set)
    rows = conn.execute("""
        SELECT tag, committed_at, deployed_at
        FROM commits
        WHERE deployed_at IS NOT NULL
        ORDER BY committed_at
    """).fetchall()
    if rows:
        print("\nTagged releases in DuckDB:")
        for tag, committed, deployed in rows:
            print(f"  {str(tag):10s}  committed {str(committed)[:10]}  deployed_at {str(deployed)[:16]}")

    conn.close()
    sys.exit(0)
