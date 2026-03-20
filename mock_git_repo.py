#!/usr/bin/env python3
"""
mock_git_repo.py

Creates a realistic git repository at data/mock_star_repo/ populated with
the STAR codebase files and ~12 months of synthetic commit history.

Release tags v2.9.0 – v2.15.0 are created at fixed offsets from today.
v2.12.0 (day -75) and v2.14.0 (day -35) are the blast-radius releases;
these dates match the BLAST_DAYS_AGO constants in mock_oracle.py.

Usage:
    python mock_git_repo.py                          # create (errors if repo already exists)
    python mock_git_repo.py --reset                  # delete and recreate
    python mock_git_repo.py --output PATH            # custom repo path
    python mock_git_repo.py --days 180               # history length in days (default: 365)
    python mock_git_repo.py --commits-per-day 5      # average commits per working day (default: 2)
    python mock_git_repo.py --reset --days 730 --commits-per-day 4  # combine
"""

import argparse
import os
import random
import shutil
import stat
from datetime import datetime, timedelta, timezone
from pathlib import Path


def _force_rmtree(path: Path):
    """Remove a directory tree, handling read-only files (common in .git on Windows)."""
    def on_error(func, fpath, _exc):
        os.chmod(fpath, stat.S_IWRITE)
        func(fpath)
    shutil.rmtree(path, onerror=on_error)

import git  # gitpython

# ---------------------------------------------------------------------------
# Shared constants — must match BLAST_DAYS_AGO in mock_oracle.py
# ---------------------------------------------------------------------------

BLAST_DAYS_AGO = (75, 35)   # v2.12.0, v2.14.0

DEFAULT_OUTPUT  = "data/mock_star_repo"
CODEBASE_SOURCE = "codebase"

# ---------------------------------------------------------------------------
# Release schedule  (days_ago, tag, is_blast_radius)
# ---------------------------------------------------------------------------

RELEASES = [
    (330, "v2.9.0",  False),
    (270, "v2.10.0", False),
    (210, "v2.11.0", False),
    (150, "v2.11.1", False),   # hotfix
    ( 75, "v2.12.0", True),    # BLAST RADIUS #1
    ( 55, "v2.13.0", False),
    ( 35, "v2.14.0", True),    # BLAST RADIUS #2
    ( 10, "v2.15.0", False),
]

# ---------------------------------------------------------------------------
# Authors
# ---------------------------------------------------------------------------

AUTHORS = [
    git.Actor("Antoine Dupont",  "a.dupont@trading-firm.com"),
    git.Actor("Sarah Chen",      "s.chen@trading-firm.com"),
    git.Actor("Marcus Webb",     "m.webb@trading-firm.com"),
    git.Actor("Priya Nair",      "p.nair@trading-firm.com"),
    git.Actor("James Fowler",    "j.fowler@trading-firm.com"),
]

# ---------------------------------------------------------------------------
# Commit recipes: (message, [relative file paths to touch])
# Files are relative to the repo root.
# ---------------------------------------------------------------------------

BLOTTER = [
    "src/services/CTradeLifecycleService.cpp",
    "src/ui/TradeForm.cpp",
    "src/services/CTradeAmendmentService.cpp",
    "include/services/CTradeLifecycleService.h",
]
SEARCH = [
    "src/core/TradeValidator.cpp",
    "src/db/CTradeOrmMapper.cpp",
    "src/db/CDBConnection.cpp",
]
CREDIT = [
    "src/services/CTradeCreditChecker.cpp",
    "src/services/CTradeRiskChecker.cpp",
    "src/services/CCreditCheckService.cpp",
    "src/services/CTradeComplianceService.cpp",
]
SETTLE = [
    "src/services/CSettlementService.cpp",
    "src/services/CTradeSettlementChecker.cpp",
    "src/services/CSettlementFinalizationService.cpp",
]
REPORT = [
    "src/services/CTradeReportingService.cpp",
    "src/services/CTradeExposureReporter.cpp",
    "src/services/CTradePositionReader.cpp",
    "src/services/CTradeExposureTracker.cpp",
]
INFRA = [
    "src/util/CLogger.cpp",
    "src/util/CConfigManager.cpp",
    "src/db/COrmBase.cpp",
]
FEE = [
    "src/services/CTradeFeeValidator.cpp",
    "src/services/CTradeFeeStore.cpp",
    "src/services/CTradeFeeSchedule.cpp",
]

RECIPES = [
    ("Optimise blotter load time for large portfolios",              BLOTTER),
    ("Fix race condition in trade lifecycle state machine",          BLOTTER),
    ("Reduce blotter refresh lock contention under load",           BLOTTER),
    ("Add blotter view filter persistence across sessions",         BLOTTER),
    ("Add trade search result pagination",                          SEARCH),
    ("Fix contract search returning stale cached results",          SEARCH),
    ("Improve ORM query plan for contract reference lookups",       SEARCH),
    ("Add credit limit pre-check before trade entry",               CREDIT),
    ("Increase credit check timeout to 5s for slow risk systems",   CREDIT),
    ("Cache credit check results for 30s to reduce load",           CREDIT),
    ("Fix settlement date calculation for CCY swaps",               SETTLE),
    ("Add LCH clearing status reconciliation",                      SETTLE),
    ("Handle novation event in settlement finalization",            SETTLE),
    ("Add position report caching layer",                           REPORT),
    ("Fix P&L report portfolio grouping regression",                REPORT),
    ("Add MTM exposure breakdown by currency",                      REPORT),
    ("Upgrade database connection pool from 10 to 20 threads",      INFRA),
    ("Improve ORM logging granularity in error paths",              INFRA),
    ("Reduce log verbosity in hot trade validation path",           INFRA),
    ("Add fee schedule version tracking",                           FEE),
    ("Fix fee threshold check for FRA product type",                FEE),
    ("Add counterparty-level fee override support",                 FEE + ["src/db/CCounterpartyOrmEntity.cpp"]),
    ("Refactor trade approval gate to support multi-level sign-off",
     ["src/services/CTradeApprovalGate.cpp", "src/services/CTradeApprovalLevelConfig.cpp"]),
    ("Add regulatory reporting for EMIR UTI allocation",
     ["src/services/CTradeRegulatoryReporter.cpp", "src/services/CTradeReportingService.cpp"]),
    ("Fix notional USD conversion for non-USD IRS books",
     ["src/services/CTradeNotionalUSDConverter.cpp", "src/services/CTradeNotionalValidator.cpp"]),
    ("Add FX rate cache invalidation on market close",
     ["src/services/CTradeFXRateCache.cpp"]),
]

# Blast radius release commit content (touches blotter + trade-entry files)
BLAST_V2_12_0_FILES = [
    "src/services/CTradeLifecycleService.cpp",
    "src/ui/TradeForm.cpp",
    "src/services/CTradeAmendmentService.cpp",
    "src/services/CTradeRateLimiter.cpp",   # new rate limiter — causes Refresh Blotter spike
]
BLAST_V2_14_0_FILES = [
    "src/core/TradeValidator.cpp",
    "src/db/CTradeOrmMapper.cpp",
    "src/ui/TradeForm.cpp",
    "src/db/CDBConnection.cpp",             # connection pool change — causes Enter New Trade spike
]


# ---------------------------------------------------------------------------
# Helpers
# ---------------------------------------------------------------------------

def _dt(days_ago: int, hour: int = 10, minute: int = 0) -> datetime:
    d = datetime.now(timezone.utc) - timedelta(days=days_ago)
    return d.replace(hour=hour, minute=minute, second=0, microsecond=0)


def _iso(dt: datetime) -> str:
    return dt.strftime("%Y-%m-%dT%H:%M:%S +0000")


def _touch_file(repo_path: Path, rel_path: str, note: str) -> bool:
    """Append a comment line to a file. Returns True if file exists."""
    full = repo_path / rel_path
    if not full.exists():
        return False
    with open(full, "a", encoding="utf-8") as f:
        f.write(f"\n// {note}\n")
    return True


def _make_commit(repo: git.Repo, repo_path: Path, files: list[str],
                 message: str, author: git.Actor, dt: datetime) -> git.Commit:
    note = f"{message[:60]}  [{author.name.split()[0]}]"
    touched = [f for f in files if _touch_file(repo_path, f, note)]
    if not touched:
        return None
    repo.index.add(touched)
    iso = _iso(dt)
    return repo.index.commit(
        message,
        author=author,
        committer=author,
        author_date=iso,
        commit_date=iso,
    )


# ---------------------------------------------------------------------------
# Main
# ---------------------------------------------------------------------------

def build_repo(output_path: str, reset: bool, days: int = 365, commits_per_day: float = 2.0):
    out = Path(output_path)

    if reset and out.exists():
        _force_rmtree(out)
        print(f"Removed {out}")

    if out.exists():
        print(f"Repo already exists at {out}. Use --reset to rebuild.")
        return

    # Copy codebase files
    src = Path(CODEBASE_SOURCE)
    if not src.exists():
        raise FileNotFoundError(f"Codebase source not found: {src}")
    shutil.copytree(src, out)
    print(f"Copied codebase to {out}")

    # Init repo with a neutral identity (won't affect user's global git config)
    repo = git.Repo.init(out)
    repo.config_writer().set_value("user", "name",  "STAR Dev Team").release()
    repo.config_writer().set_value("user", "email", "dev@trading-firm.com").release()

    today = datetime.now(timezone.utc)
    start = today - timedelta(days=days)

    # --- Initial commit (all files) ---
    repo.git.add(A=True)
    init_dt = _dt(days, hour=9)
    iso_init = _iso(init_dt)
    author0 = AUTHORS[0]
    repo.index.commit(
        "Initial commit: STAR trading platform v2.8.0",
        author=author0, committer=author0,
        author_date=iso_init, commit_date=iso_init,
    )
    print("  Initial commit done")

    # Build tag lookup: days_ago → tag info (skip releases outside the requested history window)
    release_days = {r[0]: r for r in RELEASES if r[0] < days}

    # Build a commit schedule: one entry per commit
    # Each day with commits has 0-2 regular commits + optional release commit
    schedule = []   # list of (days_ago, recipe_idx, author_idx)
    recipe_idx = 0

    for days_ago in range(days - 1, 0, -1):
        current_dt = today - timedelta(days=days_ago)
        if current_dt.weekday() >= 5:   # skip weekends mostly
            if random.random() > 0.15:
                continue

        # commits per day scaled by commits_per_day (avg on workdays)
        n_commits = max(1, round(random.gauss(commits_per_day, max(1.0, commits_per_day * 0.4))))

        for i in range(n_commits):
            hour = random.randint(9, 17)
            minute = random.randint(0, 59)
            dt = (today - timedelta(days=days_ago)).replace(
                hour=hour, minute=minute, second=0, microsecond=0,
                tzinfo=timezone.utc
            )
            schedule.append((dt, recipe_idx % len(RECIPES), random.randint(0, len(AUTHORS) - 1)))
            recipe_idx += 1

    # Sort by datetime
    schedule.sort(key=lambda x: x[0])

    # Pre-compute release commit datetimes
    release_commit_dts = {days_ago: _dt(days_ago, hour=14) for days_ago in release_days}

    # Merge release commits into schedule
    tagged_commits: dict[str, git.Commit] = {}

    print(f"  Building {len(schedule)} commits...")

    for (dt, rec_idx, auth_idx) in schedule:
        days_ago_approx = (today - dt).days

        # Inject release commit just before any scheduled commits on release days
        for rel_days, (_, tag, is_blast) in release_days.items():
            rel_dt = release_commit_dts[rel_days]
            if tag not in tagged_commits and dt > rel_dt:
                # Emit release commit
                if is_blast and tag == "v2.12.0":
                    files = BLAST_V2_12_0_FILES
                    msg = f"Release {tag}: optimise blotter refresh and add trade rate limiting"
                elif is_blast and tag == "v2.14.0":
                    files = BLAST_V2_14_0_FILES
                    msg = f"Release {tag}: rework trade entry validation pipeline and connection pooling"
                else:
                    # Pick 3-5 files from a random recipe for the release commit
                    rec = RECIPES[rel_days % len(RECIPES)]
                    files = rec[1][:4]
                    msg = f"Release {tag}"
                author = AUTHORS[rel_days % len(AUTHORS)]
                c = _make_commit(repo, out, files, msg, author, rel_dt)
                if c:
                    repo.create_tag(tag, ref=c, message=f"Release {tag}")
                    tagged_commits[tag] = c
                    print(f"  Tagged {tag} at {rel_dt.date()}")

        # Regular commit
        author = AUTHORS[auth_idx]
        recipe = RECIPES[rec_idx]
        _make_commit(repo, out, recipe[1], recipe[0], author, dt)

    # Flush any remaining release tags (e.g. v2.15.0 if schedule ended before day -10)
    for rel_days, (_, tag, is_blast) in release_days.items():
        if tag not in tagged_commits:
            rel_dt = release_commit_dts[rel_days]
            files = RECIPES[rel_days % len(RECIPES)][1][:3]
            msg = f"Release {tag}"
            author = AUTHORS[rel_days % len(AUTHORS)]
            c = _make_commit(repo, out, files, msg, author, rel_dt)
            if c:
                repo.create_tag(tag, ref=c, message=f"Release {tag}")
                tagged_commits[tag] = c
                print(f"  Tagged {tag} at {rel_dt.date()} (trailing)")

    total = len(list(repo.iter_commits()))
    print(f"\nMock git repo: {output_path}")
    print(f"  Commits: {total}")
    print(f"  Tags:    {', '.join(t.name for t in repo.tags)}")
    print(f"\nBlast radius tags (must match BLAST_DAYS_AGO in mock_oracle.py):")
    for tag in ["v2.12.0", "v2.14.0"]:
        if tag in tagged_commits:
            c = tagged_commits[tag]
            committed = datetime.fromtimestamp(c.committed_date, tz=timezone.utc)
            deployed  = committed + timedelta(hours=2)
            print(f"  {tag}: committed {committed.date()}, deployed_at ~{deployed.strftime('%Y-%m-%d %H:%M')} UTC")


if __name__ == "__main__":
    parser = argparse.ArgumentParser(description="Create mock STAR git repository")
    parser.add_argument("--output",           default=DEFAULT_OUTPUT)
    parser.add_argument("--reset",            action="store_true", help="Delete and recreate existing repo")
    parser.add_argument("--days",             type=int, default=365,
                        help="History length in days (default: 365)")
    parser.add_argument("--commits-per-day",  type=float, default=2.0, dest="commits_per_day",
                        help="Average commits per working day (default: 2)")
    args = parser.parse_args()

    out_dir = os.path.dirname(args.output)
    if out_dir:
        os.makedirs(out_dir, exist_ok=True)

    build_repo(args.output, args.reset, days=args.days, commits_per_day=args.commits_per_day)
