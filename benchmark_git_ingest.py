#!/usr/bin/env python3
"""
benchmark_git_ingest.py

Creates a large synthetic git repo via git fast-import (no working-tree I/O),
then benchmarks the git importer against it.

Defaults:
  --commits      10 000
  --files        600     (total files in the repo)
  --files-per-commit 500 (= MAX_DIFF_FILES / 2)
  --repo-path    data/bench_repo

Usage:
    python benchmark_git_ingest.py
    python benchmark_git_ingest.py --commits 10000 --files-per-commit 500
    python benchmark_git_ingest.py --reset
"""

import argparse
import os
import random
import shutil
import stat
import subprocess
import sys
import time
from datetime import datetime, timedelta, timezone
from pathlib import Path

# ── must match the importer ────────────────────────────────────────────────
MAX_DIFF_FILES = 1000


# ── helpers ────────────────────────────────────────────────────────────────

def _rmtree(p: Path):
    def _on_err(fn, path, _):
        os.chmod(path, stat.S_IWRITE)
        fn(path)
    shutil.rmtree(p, onerror=_on_err)


def _run(cmd, **kw):
    return subprocess.run(cmd, check=True, **kw)


# ── repo creation via git fast-import ──────────────────────────────────────

def build_bench_repo(
    repo_path: Path,
    n_commits: int,
    n_files: int,
    files_per_commit: int,
    reset: bool,
):
    if repo_path.exists():
        if not reset:
            print(f"Repo already exists at {repo_path}. Use --reset to rebuild.")
            return
        _rmtree(repo_path)
        print(f"Removed {repo_path}")

    repo_path.mkdir(parents=True)
    _run(["git", "init", "-q", str(repo_path)])
    _run(["git", "-C", str(repo_path), "config", "user.name",  "Bench Bot"])
    _run(["git", "-C", str(repo_path), "config", "user.email", "bench@example.com"])

    # ── build the fast-import stream in memory ─────────────────────────────
    AUTHORS = [
        ("Alice",   "alice@example.com"),
        ("Bob",     "bob@example.com"),
        ("Carol",   "carol@example.com"),
        ("Dave",    "dave@example.com"),
        ("Eve",     "eve@example.com"),
    ]

    # File list: spread across realistic-looking paths
    DIRS = ["src/core", "src/services", "src/db", "src/ui", "src/util", "include"]
    file_paths = [
        f"{DIRS[i % len(DIRS)]}/module_{i:04d}.cpp"
        for i in range(n_files)
    ]

    # Base file content (50 lines each) — used only in the initial commit
    def _base_content(path: str) -> bytes:
        lines = [f"// {path}\n"]
        lines += [f"// line {j}\n" for j in range(49)]
        return "".join(lines).encode()

    start_ts = int((datetime.now(timezone.utc) - timedelta(days=n_commits // 20)).timestamp())
    mark = [0]

    def _next_mark():
        mark[0] += 1
        return mark[0]

    stream_parts = []

    def w(s: str):
        stream_parts.append(s.encode())

    def wb(b: bytes):
        stream_parts.append(b)

    print(f"Building fast-import stream: {n_commits} commits × {files_per_commit} files …")

    # ── initial commit (all files) ─────────────────────────────────────────
    m = _next_mark()
    ts = start_ts
    w(f"commit refs/heads/master\nmark :{m}\n")
    w(f"committer Bench Bot <bench@example.com> {ts} +0000\n")
    msg = b"Initial commit"
    w(f"data {len(msg)}\n")
    wb(msg)
    # no blank line here — file modifications follow immediately
    for fp in file_paths:
        content = _base_content(fp)
        w(f"M 100644 inline {fp}\n")
        w(f"data {len(content)}\n")
        wb(content)
        # no trailing newline after inline data — length is exact
    w("\n")  # blank line terminates commit

    # ── subsequent commits ─────────────────────────────────────────────────
    rng = random.Random(42)
    for i in range(1, n_commits):
        m = _next_mark()
        ts = start_ts + i * 60          # one commit per minute
        author, email = AUTHORS[i % len(AUTHORS)]
        w(f"commit refs/heads/master\nmark :{m}\n")
        w(f"committer {author} <{email}> {ts} +0000\n")
        msg = f"chore: batch update #{i}".encode()
        w(f"data {len(msg)}\n")
        wb(msg)
        w(f"from :{m - 1}\n")  # single newline — file mods follow immediately

        changed = rng.sample(file_paths, min(files_per_commit, n_files))
        for fp in changed:
            content = f"// {fp}\n// commit {i}\n// line {rng.randint(0, 9999)}\n".encode()
            w(f"M 100644 inline {fp}\n")
            w(f"data {len(content)}\n")
            wb(content)
            # no trailing newline after inline data
        w("\n")  # blank line terminates commit

    stream = b"".join(stream_parts)
    print(f"  Stream size: {len(stream) / 1_048_576:.1f} MB")

    t0 = time.monotonic()
    proc = _run(
        ["git", "-C", str(repo_path), "fast-import", "--quiet"],
        input=stream,
    )
    elapsed = time.monotonic() - t0
    print(f"  git fast-import done in {elapsed:.1f}s")

    # Checkout HEAD so gitpython can read the working tree
    _run(["git", "-C", str(repo_path), "checkout", "-q", "master"])

    total = int(subprocess.check_output(
        ["git", "-C", str(repo_path), "rev-list", "--count", "HEAD"]
    ).strip())
    print(f"  Commits in repo: {total}")
    return total


# ── run the git importer ───────────────────────────────────────────────────

def run_importer(repo_path: Path, max_commits: int):
    # Import inline to avoid polluting the real DB
    import duckdb
    from src.ingestion.git_importer import import_repo

    conn = duckdb.connect(":memory:")
    conn.execute("""
        CREATE TABLE commits (
            commit_hash  TEXT PRIMARY KEY,
            repo_name    TEXT,
            author_name  TEXT,
            author_email TEXT,
            committed_at TEXT,
            message      TEXT,
            is_merge     BOOLEAN,
            tag          TEXT,
            deployed_at  TEXT
        )
    """)
    conn.execute("""
        CREATE TABLE commit_files (
            commit_hash  TEXT,
            file_path    TEXT,
            change_type  TEXT,
            lines_added  INTEGER,
            lines_removed INTEGER
        )
    """)

    repo_cfg = {"path": str(repo_path), "name": "bench-repo"}

    print(f"\nRunning git importer (max_commits={max_commits}) …")
    t0 = time.monotonic()
    result = import_repo(repo_cfg, conn, max_commits=max_commits, dry_run=False)
    elapsed = time.monotonic() - t0

    n_commits = conn.execute("SELECT COUNT(*) FROM commits").fetchone()[0]
    n_files   = conn.execute("SELECT COUNT(*) FROM commit_files").fetchone()[0]
    conn.close()

    return {
        "imported":  result["imported"],
        "skipped":   result["skipped"],
        "db_commits": n_commits,
        "db_files":   n_files,
        "elapsed_s":  elapsed,
    }


# ── main ───────────────────────────────────────────────────────────────────

def main():
    ap = argparse.ArgumentParser(description="Benchmark git ingestion")
    ap.add_argument("--commits",          type=int,   default=10_000)
    ap.add_argument("--files",            type=int,   default=600)
    ap.add_argument("--files-per-commit", type=int,   default=MAX_DIFF_FILES // 2,
                    dest="files_per_commit")
    ap.add_argument("--repo-path",        default="data/bench_repo")
    ap.add_argument("--reset",            action="store_true")
    ap.add_argument("--skip-build",       action="store_true",
                    help="Skip repo creation (use existing bench_repo)")
    args = ap.parse_args()

    repo_path = Path(args.repo_path)

    if not args.skip_build:
        t0 = time.monotonic()
        total = build_bench_repo(
            repo_path,
            n_commits=args.commits,
            n_files=args.files,
            files_per_commit=args.files_per_commit,
            reset=args.reset,
        )
        build_time = time.monotonic() - t0
        print(f"  Repo build total: {build_time:.1f}s")
    else:
        print(f"Skipping repo build, using {repo_path}")

    stats = run_importer(repo_path, max_commits=args.commits)

    elapsed = stats["elapsed_s"]
    imported = stats["imported"]
    rate = imported / elapsed if elapsed > 0 else 0

    print("\n" + "=" * 50)
    print("BENCHMARK RESULTS")
    print("=" * 50)
    print(f"  Commits imported : {imported:>8,}")
    print(f"  Commits skipped  : {stats['skipped']:>8,}")
    print(f"  File rows        : {stats['db_files']:>8,}")
    print(f"  Elapsed          : {elapsed:>8.1f}s")
    print(f"  Throughput       : {rate:>8.1f} commits/s")
    if imported > 0:
        print(f"  Per-commit avg   : {elapsed / imported * 1000:>8.1f} ms")
    print("=" * 50)
    if imported >= 10_000:
        eta_153k = 153_000 / rate / 60
        print(f"  Projected 153k   : ~{eta_153k:.0f} min at this rate")
    print()


if __name__ == "__main__":
    main()
