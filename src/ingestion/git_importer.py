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
import sys
import time
from datetime import datetime, timedelta, timezone
from pathlib import Path

import git
import yaml

from src.db.connection import get_connection

SETTINGS_PATH  = "config/settings.yaml"
MAX_DIFF_FILES = 1000   # skip file-level diff if commit touches more than this


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
# File diff extraction
# ---------------------------------------------------------------------------

def extract_file_diffs(commit: git.Commit) -> list[dict] | None:
    """
    Returns list of {file_path, change_type, lines_added, lines_removed}.
    Returns None if diff is too large or commit is the initial commit.
    """
    if not commit.parents:
        # Initial commit: treat all files as Added
        diffs = []
        for item in commit.tree.traverse():
            if item.type == "blob":
                diffs.append({
                    "file_path":     item.path,
                    "change_type":   "A",
                    "lines_added":   0,
                    "lines_removed": 0,
                })
                if len(diffs) >= MAX_DIFF_FILES:
                    return None
        return diffs

    parent = commit.parents[0]
    try:
        diffs_raw = parent.diff(commit, create_patch=False)
    except Exception:
        return None

    if len(diffs_raw) > MAX_DIFF_FILES:
        return None

    result = []
    for d in diffs_raw:
        change_type = d.change_type[0].upper()  # A, M, D, R
        path = d.b_path or d.a_path
        # Line counts require patch; use stats for efficiency
        result.append({
            "file_path":     path,
            "change_type":   change_type,
            "lines_added":   None,
            "lines_removed": None,
        })

    # Get line counts from stats (one pass for the whole commit)
    try:
        stats = commit.stats.files
        for item in result:
            st = stats.get(item["file_path"])
            if st:
                item["lines_added"]   = st.get("insertions", 0)
                item["lines_removed"] = st.get("deletions",  0)
    except Exception:
        pass

    return result


# ---------------------------------------------------------------------------
# DuckDB insert
# ---------------------------------------------------------------------------

def insert_commit(conn, commit_row: dict, file_rows: list[dict]) -> None:
    conn.execute("""
        INSERT OR IGNORE INTO commits
            (commit_hash, repo_name, author_name, author_email,
             committed_at, message, is_merge, tag, deployed_at)
        VALUES (?, ?, ?, ?, ?, ?, ?, ?, ?)
    """, [
        commit_row["commit_hash"],
        commit_row["repo_name"],
        commit_row["author_name"],
        commit_row["author_email"],
        commit_row["committed_at"],
        commit_row["message"],
        commit_row["is_merge"],
        commit_row["tag"],
        commit_row["deployed_at"],
    ])

    if file_rows:
        conn.executemany("""
            INSERT INTO commit_files
                (commit_hash, file_path, change_type, lines_added, lines_removed)
            VALUES (?, ?, ?, ?, ?)
        """, [
            (commit_row["commit_hash"], r["file_path"], r["change_type"],
             r["lines_added"], r["lines_removed"])
            for r in file_rows
        ])


# ---------------------------------------------------------------------------
# Per-repo import
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

    try:
        repo = git.Repo(repo_path)
    except git.InvalidGitRepositoryError:
        print(f"  [{repo_name}] not a git repository: {repo_path} — skipping")
        return {"repo": repo_name, "imported": 0, "skipped": 0, "error": "not a git repo"}

    known   = get_known_hashes(conn)
    tag_map = build_tag_map(repo)

    imported = 0
    skipped  = 0

    for commit in repo.iter_commits():
        if max_commits and (imported + skipped) >= max_commits:
            break

        if commit.hexsha in known:
            skipped += 1
            continue

        committed_at = datetime.fromtimestamp(commit.committed_date, tz=timezone.utc)

        commit_row = {
            "commit_hash":  commit.hexsha,
            "repo_name":    repo_name,
            "author_name":  commit.author.name,
            "author_email": commit.author.email,
            "committed_at": committed_at.isoformat(),
            "message":      commit.message[:1000],
            "is_merge":     len(commit.parents) > 1,
            "tag":          tag_map.get(commit.hexsha),   # only direct tag; nearest resolved by analytics layer
            "deployed_at":  _deployed_at_str(commit, tag_map),
        }

        if dry_run:
            imported += 1
            continue

        try:
            file_diffs = extract_file_diffs(commit)
        except Exception as e:
            print(f"    [warn] diff failed for {commit.hexsha[:8]}: {e}")
            file_diffs = None

        insert_commit(conn, commit_row, file_diffs or [])
        imported += 1

    if not dry_run:
        conn.commit() if hasattr(conn, "commit") else None

    return {"repo": repo_name, "imported": imported, "skipped": skipped}


def _deployed_at_str(commit: git.Commit, tag_map: dict[str, str]) -> str | None:
    dt = estimate_deployed_at(commit, tag_map)
    return dt.isoformat() if dt else None


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
