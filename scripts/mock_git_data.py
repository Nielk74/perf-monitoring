"""
scripts/mock_git_data.py

Generates realistic mock git commit data (commits + commit_files) for 1 year
into perf_monitor.duckdb. Aligned with the actual STAR feature set.

Usage:
    python scripts/mock_git_data.py
    python scripts/mock_git_data.py --days 365 --db data/perf_monitor.duckdb
"""

import argparse
import hashlib
import random
from datetime import datetime, timedelta, timezone

from src.db.connection import get_connection

# ---------------------------------------------------------------------------
# Configuration
# ---------------------------------------------------------------------------

REPO_NAME = "star-app"

AUTHORS = [
    ("Alice Martin",    "alice.martin@company.com"),
    ("Bob Nguyen",      "bob.nguyen@company.com"),
    ("Clara Dubois",    "clara.dubois@company.com"),
    ("David Schmidt",   "david.schmidt@company.com"),
    ("Emma Leclerc",    "emma.leclerc@company.com"),
    ("François Petit",  "francois.petit@company.com"),
]

# File paths mapped to feature areas in audit_events
FILE_AREAS = {
    "blotter": [
        "src/blotter/EnterTrade.java",
        "src/blotter/CancelTrade.java",
        "src/blotter/AmendTrade.java",
        "src/blotter/BlotterView.java",
        "src/blotter/RefreshBlotter.java",
        "src/blotter/BlotterService.java",
        "src/blotter/TradeValidator.java",
        "tests/blotter/EnterTradeTest.java",
        "tests/blotter/CancelTradeTest.java",
    ],
    "report": [
        "src/report/PnLReport.java",
        "src/report/PositionReport.java",
        "src/report/LegacyAuditTrailReport.java",
        "src/report/ReportScheduler.java",
        "src/report/ReportExporter.java",
        "tests/report/PnLReportTest.java",
    ],
    "search": [
        "src/search/PortfolioSearch.java",
        "src/search/ContractSearch.java",
        "src/search/SearchIndex.java",
        "src/search/SearchService.java",
    ],
    "static": [
        "src/static/CounterpartyEditor.java",
        "src/static/CounterpartyViewer.java",
        "src/static/ArchiveDataViewer.java",
        "src/static/StaticDataService.java",
    ],
    "tools": [
        "src/tools/ExcelExporter.java",
        "src/tools/BatchProcessor.java",
        "src/tools/CopyContractNo.java",
        "src/tools/ToolsMenu.java",
    ],
    "system": [
        "src/system/StarStartup.java",
        "src/system/AppConfig.java",
        "src/system/SessionManager.java",
        "src/system/HealthCheck.java",
    ],
    "infra": [
        "pom.xml",
        "config/application.properties",
        "config/logback.xml",
        "scripts/deploy.sh",
        "Dockerfile",
        ".github/workflows/ci.yml",
    ],
    "db": [
        "db/migrations/V001__init_schema.sql",
        "db/migrations/V002__add_trade_status.sql",
        "db/migrations/V003__index_blotter.sql",
        "db/migrations/V004__pnl_view.sql",
        "db/migrations/V005__counterparty_audit.sql",
    ],
}

COMMIT_TEMPLATES = [
    ("fix: {area} - correct {detail}", False),
    ("feat: {area} - add {detail}", False),
    ("refactor: {area} - simplify {detail}", False),
    ("perf: {area} - optimise {detail}", False),
    ("test: {area} - add unit tests for {detail}", False),
    ("chore: bump dependencies", False),
    ("docs: update README for {area}", False),
    ("fix: regression in {area} after {detail}", False),
    ("Merge branch 'feature/{area}-{detail}' into master", True),
    ("Merge pull request #NNN from {area}/{detail}", True),
]

DETAILS = [
    "trade validation", "P&L calculation", "search indexing", "session timeout",
    "blotter refresh", "counterparty lookup", "Excel export", "batch processing",
    "audit logging", "position report", "cancel workflow", "amend logic",
    "startup sequence", "config loading", "DB connection pool", "cache eviction",
    "permission check", "trade status", "portfolio label", "contract copy",
]


def fake_hash(seed: str) -> str:
    return hashlib.sha1(seed.encode()).hexdigest()


def pick_files(rng: random.Random) -> list[tuple]:
    """Pick 1-6 files from random areas with realistic change sizes."""
    area_keys = list(FILE_AREAS.keys())
    n_areas = rng.randint(1, 3)
    chosen_areas = rng.sample(area_keys, min(n_areas, len(area_keys)))
    files = []
    for area in chosen_areas:
        pool = FILE_AREAS[area]
        n = rng.randint(1, min(3, len(pool)))
        for path in rng.sample(pool, n):
            change_type = rng.choices(["M", "A", "D"], weights=[70, 20, 10])[0]
            added   = rng.randint(0, 120) if change_type != "D" else 0
            removed = rng.randint(0, 60)  if change_type != "A" else 0
            files.append((path, change_type, added, removed))
    return files


def generate_commits(days: int, rng: random.Random) -> tuple[list, list]:
    now = datetime.now(tz=timezone.utc)
    start = now - timedelta(days=days)

    commit_rows = []
    file_rows   = []
    seen_hashes: set[str] = set()

    # ~3-7 commits per working day on average
    current = start
    idx = 0
    while current <= now:
        # Skip weekends with 80% probability (some weekend commits)
        if current.weekday() >= 5 and rng.random() < 0.80:
            current += timedelta(days=1)
            continue

        n_commits = rng.randint(2, 8)
        for _ in range(n_commits):
            # Random time during working hours (7h-19h UTC)
            offset_secs = rng.randint(0, 86400)
            committed_at = current + timedelta(seconds=offset_secs)
            if committed_at > now:
                continue

            author = rng.choice(AUTHORS)
            area   = rng.choice(list(FILE_AREAS.keys()))
            detail = rng.choice(DETAILS)
            tmpl, is_merge = rng.choice(COMMIT_TEMPLATES)
            message = tmpl.format(area=area, detail=detail)

            seed = f"{idx}:{committed_at.isoformat()}:{author[0]}"
            h = fake_hash(seed)
            if h in seen_hashes:
                idx += 1
                continue
            seen_hashes.add(h)

            # Occasional release tags (~every 2 weeks)
            tag = None
            if idx % 70 == 0:
                tag = f"v1.{idx // 70}.0"

            deployed_at = (committed_at + timedelta(days=1)).isoformat()

            commit_rows.append((
                h, REPO_NAME,
                author[0], author[1],
                committed_at.isoformat(),
                message,
                is_merge,
                tag,
                deployed_at,
            ))

            files = pick_files(rng)
            for path, ctype, added, removed in files:
                file_rows.append((h, path, ctype, added, removed))

            idx += 1

        current += timedelta(days=1)

    return commit_rows, file_rows


def main():
    parser = argparse.ArgumentParser()
    parser.add_argument("--days",  type=int, default=365)
    parser.add_argument("--db",    default=None)
    parser.add_argument("--seed",  type=int, default=42)
    args = parser.parse_args()

    rng = random.Random(args.seed)
    print(f"Generating mock git data for {args.days} days (seed={args.seed}) …")
    commit_rows, file_rows = generate_commits(args.days, rng)
    print(f"  {len(commit_rows):,} commits, {len(file_rows):,} file rows")

    conn = get_connection(args.db) if args.db else get_connection()

    # Clear existing mock data for this repo (idempotent re-run)
    existing = conn.execute(
        "SELECT COUNT(*) FROM commits WHERE repo_name = ?", [REPO_NAME]
    ).fetchone()[0]
    if existing:
        print(f"  Clearing {existing:,} existing rows for repo '{REPO_NAME}' …")
        hashes = [r[0] for r in conn.execute(
            "SELECT commit_hash FROM commits WHERE repo_name = ?", [REPO_NAME]
        ).fetchall()]
        conn.execute("DELETE FROM commits WHERE repo_name = ?", [REPO_NAME])
        if hashes:
            placeholders = ",".join(["?"] * len(hashes))
            conn.execute(f"DELETE FROM commit_files WHERE commit_hash IN ({placeholders})", hashes)

    print("  Inserting commits …")
    conn.executemany(
        """INSERT INTO commits
           (commit_hash, repo_name, author_name, author_email,
            committed_at, message, is_merge, tag, deployed_at)
           VALUES (?,?,?,?,?,?,?,?,?)""",
        commit_rows,
    )

    print("  Inserting commit_files …")
    conn.executemany(
        """INSERT INTO commit_files
           (commit_hash, file_path, change_type, lines_added, lines_removed)
           VALUES (?,?,?,?,?)""",
        file_rows,
    )

    conn.close()

    print("Done.")
    print(f"  commits     : {len(commit_rows):,}")
    print(f"  commit_files: {len(file_rows):,}")


if __name__ == "__main__":
    main()
