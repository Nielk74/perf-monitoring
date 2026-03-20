#!/usr/bin/env python3
"""
mock_oracle.py

Creates a SQLite database that mirrors the Oracle STAR.STAR_ACTION_AUDIT table,
populated with synthetic data covering all 10 performance monitoring use cases.

Usage:
    python mock_oracle.py                             # medium scale (default)
    python mock_oracle.py --scale small               # 3 months, ~100k events
    python mock_oracle.py --scale large               # 12 months, ~2M events
    python mock_oracle.py --reset                     # truncate and regenerate
    python mock_oracle.py --output PATH               # custom db path (default: data/mock_oracle.db)

    # Fine-grained overrides (applied on top of the chosen --scale profile):
    python mock_oracle.py --users 500                 # override number of users
    python mock_oracle.py --days 60                   # override history length in days
    python mock_oracle.py --events-per-day 20000      # override event volume per day
    python mock_oracle.py --scale large --users 1000 --events-per-day 50000  # combine
"""

import argparse
import calendar
import math
import os
import random
import sqlite3
import sys
from datetime import date, datetime, timedelta, timezone

# ---------------------------------------------------------------------------
# Scale profiles
# ---------------------------------------------------------------------------

SCALE_PROFILES = {
    "small":  {"users": 20,  "workstations": 30,  "days": 90,  "events_per_day": 500},
    "medium": {"users": 80,  "workstations": 120, "days": 180, "events_per_day": 5000},
    "large":  {"users": 200, "workstations": 350, "days": 365, "events_per_day": 15000},
}

DEFAULT_SCALE  = "medium"
DEFAULT_OUTPUT = "data/mock_oracle.db"

# ---------------------------------------------------------------------------
# Feature taxonomy
# (feature_type, feature, weight, has_duration, base_duration_ms, stddev_ms)
#
# Weights are relative — higher = more frequent.
# Zombie features (UC-05) are suppressed past the dataset's halfway point.
# "New Blotter View" starts at weight 0 and is injected via S-curve (UC-08).
# ---------------------------------------------------------------------------

FEATURES = [
    ("SYSTEM",   "Star Startup",                   1,  True,  120000, 30000),
    ("SEARCH",   "Search Contract",               15,  True,     800,   400),
    ("SEARCH",   "Search Portfolio",              10,  True,    1200,   600),
    ("BLOTTER",  "Open Blotter",                   8,  True,    2500,  1200),
    ("BLOTTER",  "Refresh Blotter",               12,  True,    3500,  2000),
    ("BLOTTER",  "Enter New Trade",                6,  True,    4000,  1500),
    ("BLOTTER",  "Amend Trade",                    5,  True,    3000,  1000),
    ("BLOTTER",  "Cancel Trade",                   3,  True,    2000,   800),
    ("TOOLS",    "Copy Contract No",              20, False,    None,  None),
    ("TOOLS",    "Export to Excel",                6,  True,    4500,  1500),
    ("REPORT",   "Run P&L Report",                 5,  True,   15000,  5000),
    ("REPORT",   "Run Position Report",            4,  True,   12000,  4000),
    ("STATIC",   "Open Counterparty",              7,  True,    2000,   800),
    ("STATIC",   "Edit Counterparty",              3,  True,    1500,   500),
    ("PTFLABEL", "ShowPortfolioLabel Permission",  2, False,    None,  None),
    # UC-05 zombies — suppressed after the halfway point
    ("REPORT",   "Legacy Audit Trail Report",      1,  True,   45000, 10000),
    ("TOOLS",    "Old Batch Processor",            1,  True,   60000, 20000),
    ("STATIC",   "Archive Data Viewer",            1,  True,    3000,  1000),
    # UC-08 new feature — not in base pool; injected via S-curve adoption weight
    ("BLOTTER",  "New Blotter View",               0,  True,    2000,   600),
]

FEATURE_MAP = {f[1]: f for f in FEATURES}

ZOMBIE_FEATURES       = {"Legacy Audit Trail Report", "Old Batch Processor", "Archive Data Viewer"}
DEGRADING_FEATURES    = {"Refresh Blotter", "Run P&L Report", "Open Blotter"}   # UC-01
BLAST_RADIUS_FEATURES = {"Refresh Blotter", "Enter New Trade"}                   # UC-02

# ---------------------------------------------------------------------------
# Office config  (busy_start/end are UTC hours)
# ---------------------------------------------------------------------------

OFFICES = {
    # LON: 08-17 local = 08-17 UTC
    "LON": {"busy_start":  8, "busy_end": 17},
    # NYC: 09-18 local = 14-23 UTC
    "NYC": {"busy_start": 14, "busy_end": 23},
    # SIN: 09-18 local = 01-10 UTC
    "SIN": {"busy_start":  1, "busy_end": 10},
}

USER_PREFIXES = [
    "MS", "JD", "RK", "AB", "TP", "LM", "CP", "DW", "NK", "GH",
    "FS", "BT", "MC", "PL", "AR", "SW", "NB", "JF", "EV", "OC",
]

PRODUCT_TYPES = ["IRS", "CCY", "FRA", "CAP", "FLR", "SWPTN"]
BOOKS         = ["RATES-01", "RATES-02", "XCC-01", "DERIV-A", "DERIV-B", "HY-DESK"]
PORTFOLIOS    = ["PTF-ALPHA", "PTF-BETA", "PTF-GAMMA", "PTF-DELTA", "PTF-HY"]


# ---------------------------------------------------------------------------
# Helpers
# ---------------------------------------------------------------------------

def weighted_choice(names, weights):
    total = sum(weights)
    r = random.random() * total
    cumulative = 0.0
    for name, w in zip(names, weights):
        cumulative += w
        if r < cumulative:
            return name
    return names[-1]


def sample_duration(base_ms, stddev_ms):
    return max(50, int(random.gauss(base_ms, stddev_ms)))


def last_two_business_days(d):
    """Return the set of last 2 business days of the month containing `d`."""
    last = calendar.monthrange(d.year, d.month)[1]
    result = []
    for day in range(last, last - 7, -1):
        candidate = date(d.year, d.month, day)
        if candidate.weekday() < 5:
            result.append(candidate)
        if len(result) == 2:
            break
    return set(result)


# ---------------------------------------------------------------------------
# Duration modifiers
# ---------------------------------------------------------------------------

def degradation_factor(feature, current, end):
    """UC-01: +6 %/week drift on DEGRADING_FEATURES over the last 10 weeks."""
    if feature not in DEGRADING_FEATURES:
        return 1.0
    onset = end - timedelta(weeks=10)
    if current < onset:
        return 1.0
    weeks = (current - onset).days / 7.0
    return 1.06 ** weeks


def blast_radius_factor(feature, current, blast_dates):
    """UC-02: 2× duration in the 48-hour window after each problematic deploy."""
    if feature not in BLAST_RADIUS_FEATURES:
        return 1.0
    for deploy in blast_dates:
        if deploy <= current <= deploy + timedelta(days=2):
            return 2.0
    return 1.0


def lon_startup_factor(feature, office):
    """UC-03: LON workstations have +40 % startup time."""
    return 1.40 if (feature == "Star Startup" and office == "LON") else 1.0


def impersonation_factor(is_imp):
    """UC-04: support sessions are 15 % faster."""
    return 0.85 if is_imp else 1.0


def nbv_adoption_weight(current, end):
    """
    UC-08: S-curve adoption for New Blotter View.
    Introduced 90 days before `end`. Returns 0 before intro date,
    grows logistically to ~5 (matching base feature weight scale) at plateau.
    """
    intro = end - timedelta(days=90)
    days = (current - intro).days
    if days < 0:
        return 0.0
    return 5.0 / (1.0 + math.exp(-0.15 * (days - 25)))


def make_detail(feature):
    if feature in {"Search Contract", "Amend Trade", "Cancel Trade", "Copy Contract No"}:
        return f"{random.choice(PRODUCT_TYPES)}-{random.randint(100000, 999999)}"
    if feature == "Search Portfolio":
        return random.choice(PORTFOLIOS)
    if feature in {"Open Blotter", "Refresh Blotter", "New Blotter View"}:
        return random.choice(BOOKS)
    if feature == "Enter New Trade":
        return random.choice(PRODUCT_TYPES)
    if feature in {"Run P&L Report", "Run Position Report"}:
        return random.choice(PORTFOLIOS)
    if feature in {"Open Counterparty", "Edit Counterparty"}:
        return f"CPTY-{random.randint(1000, 9999)}"
    return None


def make_timestamp(d, office):
    info = OFFICES[office]
    if random.random() < 0.80:
        hour = random.randint(info["busy_start"], info["busy_end"] - 1) % 24
    else:
        hour = random.randint(0, 23)
    return datetime(d.year, d.month, d.day,
                    hour, random.randint(0, 59), random.randint(0, 59),
                    tzinfo=timezone.utc)


# ---------------------------------------------------------------------------
# Population builder
# ---------------------------------------------------------------------------

def build_population(params):
    """
    Returns:
        normal_users  list[str]
        support_users list[str]   (5 accounts prefixed "supp_")
        user_office   dict[str -> str]
        workstations  dict[str -> list[str]]
    """
    total_u = params["users"]
    total_w = params["workstations"]
    offices = list(OFFICES.keys())
    ratios  = [50, 30, 20]
    ratio_s = sum(ratios)

    u_counts = {}
    w_counts = {}
    alloc_u = alloc_w = 0
    for i, off in enumerate(offices[:-1]):
        nu = max(1, int(total_u * ratios[i] / ratio_s))
        nw = max(1, int(total_w * ratios[i] / ratio_s))
        u_counts[off] = nu
        w_counts[off] = nw
        alloc_u += nu
        alloc_w += nw
    u_counts[offices[-1]] = max(1, total_u - alloc_u)
    w_counts[offices[-1]] = max(1, total_w - alloc_w)

    prefixes = (USER_PREFIXES * ((total_u // len(USER_PREFIXES)) + 2))
    random.shuffle(prefixes)

    normal_users = []
    user_office  = {}
    idx = 0
    for off in offices:
        for n in range(u_counts[off]):
            name = f"{prefixes[idx]}{n+1:02d}"
            idx += 1
            normal_users.append(name)
            user_office[name] = off

    workstations = {
        off: [f"{off}W{random.randint(10000000, 99999999):08d}" for _ in range(w_counts[off])]
        for off in offices
    }

    support_users = [f"supp_{i+1:02d}" for i in range(5)]
    return normal_users, support_users, user_office, workstations


# ---------------------------------------------------------------------------
# SQLite schema + flush
# ---------------------------------------------------------------------------

def create_schema(conn):
    conn.execute("""
        CREATE TABLE IF NOT EXISTS STAR_ACTION_AUDIT (
            SUPP_USER    TEXT NOT NULL,
            ASMD_USER    TEXT NOT NULL,
            WORKSTATION  TEXT NOT NULL,
            MOD_DT       TEXT NOT NULL,
            FEATURE_TYPE TEXT NOT NULL,
            FEATURE      TEXT NOT NULL,
            DETAIL       TEXT,
            DURATION_MS  INTEGER
        )
    """)
    conn.execute("CREATE INDEX IF NOT EXISTS idx_saa_mod_dt    ON STAR_ACTION_AUDIT(MOD_DT)")
    conn.execute("CREATE INDEX IF NOT EXISTS idx_saa_asmd_user ON STAR_ACTION_AUDIT(ASMD_USER)")
    conn.execute("CREATE INDEX IF NOT EXISTS idx_saa_feature   ON STAR_ACTION_AUDIT(FEATURE)")
    conn.commit()


def flush(conn, batch):
    conn.executemany("""
        INSERT INTO STAR_ACTION_AUDIT
            (SUPP_USER, ASMD_USER, WORKSTATION, MOD_DT,
             FEATURE_TYPE, FEATURE, DETAIL, DURATION_MS)
        VALUES (?, ?, ?, ?, ?, ?, ?, ?)
    """, batch)
    conn.commit()


# ---------------------------------------------------------------------------
# Verification summary
# ---------------------------------------------------------------------------

def verify(conn, today, end, halfway, blast_dates, anomaly_count_date, anomaly_duration_date):
    print("\nUse-case coverage check:")

    # UC-01
    for feature in sorted(DEGRADING_FEATURES):
        onset = (end - timedelta(weeks=10)).isoformat()
        recent = conn.execute(
            "SELECT AVG(DURATION_MS) FROM STAR_ACTION_AUDIT WHERE FEATURE=? AND DATE(MOD_DT)>=?",
            (feature, onset)).fetchone()[0]
        base = conn.execute(
            "SELECT AVG(DURATION_MS) FROM STAR_ACTION_AUDIT WHERE FEATURE=? AND DATE(MOD_DT)<?",
            (feature, onset)).fetchone()[0]
        if recent and base and base > 0:
            pct = (recent - base) / base * 100
            print(f"  [UC-01] '{feature}': base {int(base)} ms -> recent {int(recent)} ms ({pct:+.1f} %)")

    # UC-02
    for deploy in blast_dates:
        win_end = (deploy + timedelta(days=2)).isoformat()
        row = conn.execute("""
            SELECT COUNT(*), AVG(DURATION_MS) FROM STAR_ACTION_AUDIT
            WHERE FEATURE IN ('Refresh Blotter','Enter New Trade')
              AND DATE(MOD_DT) BETWEEN ? AND ?
        """, (deploy.isoformat(), win_end)).fetchone()
        print(f"  [UC-02] Deploy {deploy}: {row[0]:,} blast-radius events, avg {int(row[1] or 0)} ms")

    # UC-03
    lon = conn.execute(
        "SELECT AVG(DURATION_MS) FROM STAR_ACTION_AUDIT WHERE FEATURE='Star Startup' AND WORKSTATION LIKE 'LON%'"
    ).fetchone()[0]
    non_lon = conn.execute(
        "SELECT AVG(DURATION_MS) FROM STAR_ACTION_AUDIT WHERE FEATURE='Star Startup' AND WORKSTATION NOT LIKE 'LON%'"
    ).fetchone()[0]
    if lon and non_lon:
        print(f"  [UC-03] LON startup {int(lon)} ms vs non-LON {int(non_lon)} ms ({(lon/non_lon-1)*100:+.1f} %)")

    # UC-04
    imp = conn.execute("SELECT COUNT(*) FROM STAR_ACTION_AUDIT WHERE SUPP_USER != ASMD_USER").fetchone()[0]
    print(f"  [UC-04] Impersonated events: {imp:,}")

    # UC-05 — zombie features should have zero events after the halfway point
    for z in sorted(ZOMBIE_FEATURES):
        cnt = conn.execute(
            "SELECT COUNT(*) FROM STAR_ACTION_AUDIT WHERE FEATURE=? AND DATE(MOD_DT)>?",
            (z, halfway.isoformat())).fetchone()[0]
        status = "OK" if cnt == 0 else "FAIL"
        print(f"  [UC-05] [{status}] '{z}': {cnt} events after {halfway} (expect 0)")

    # UC-06 (top friction features by total wait)
    rows = conn.execute("""
        SELECT FEATURE, SUM(DURATION_MS) total_wait
        FROM STAR_ACTION_AUDIT WHERE DURATION_MS IS NOT NULL
        GROUP BY FEATURE ORDER BY total_wait DESC LIMIT 3
    """).fetchall()
    for feat, wait in rows:
        print(f"  [UC-06] Friction: '{feat}' - {wait/1e9:.2f}B ms total wait")

    # UC-08
    nbv = conn.execute(
        "SELECT COUNT(*), COUNT(DISTINCT ASMD_USER) FROM STAR_ACTION_AUDIT WHERE FEATURE='New Blotter View'"
    ).fetchone()
    print(f"  [UC-08] 'New Blotter View': {nbv[0]:,} events, {nbv[1]} distinct users")

    # UC-09
    prev_month_last = today.replace(day=1) - timedelta(days=1)
    me_days = last_two_business_days(prev_month_last)
    if me_days:
        sample = min(me_days).isoformat()
        me_cnt = conn.execute(
            "SELECT COUNT(*) FROM STAR_ACTION_AUDIT WHERE DATE(MOD_DT)=?", (sample,)
        ).fetchone()[0]
        avg_cnt = conn.execute("""
            SELECT AVG(cnt) FROM (
                SELECT DATE(MOD_DT) d, COUNT(*) cnt FROM STAR_ACTION_AUDIT
                WHERE strftime('%w', MOD_DT) NOT IN ('0','6')
                GROUP BY d
            )
        """).fetchone()[0]
        ratio = me_cnt / avg_cnt if avg_cnt else 0
        print(f"  [UC-09] Month-end {sample}: {me_cnt:,} events vs avg {int(avg_cnt or 0)} ({ratio:.1f}x)")

    # UC-10
    for label, adate in [("count spike", anomaly_count_date), ("duration spike", anomaly_duration_date)]:
        cnt = conn.execute(
            "SELECT COUNT(*) FROM STAR_ACTION_AUDIT WHERE DATE(MOD_DT)=?", (adate.isoformat(),)
        ).fetchone()[0]
        avg = conn.execute("""
            SELECT AVG(cnt) FROM (
                SELECT DATE(MOD_DT) d, COUNT(*) cnt FROM STAR_ACTION_AUDIT GROUP BY d
            )
        """).fetchone()[0]
        z = (cnt - avg) / (avg ** 0.5) if avg else 0
        print(f"  [UC-10] Anomaly ({label}) on {adate}: {cnt:,} events (z ~{z:.1f})")


# ---------------------------------------------------------------------------
# Entry point
# ---------------------------------------------------------------------------

if __name__ == "__main__":
    parser = argparse.ArgumentParser(description="Generate mock Oracle STAR_ACTION_AUDIT in SQLite")
    parser.add_argument("--scale",          choices=SCALE_PROFILES.keys(), default=DEFAULT_SCALE)
    parser.add_argument("--output",         default=DEFAULT_OUTPUT)
    parser.add_argument("--reset",          action="store_true", help="Drop and regenerate existing database")
    parser.add_argument("--users",          type=int, help="Override number of users (e.g. 500)")
    parser.add_argument("--days",           type=int, help="Override history length in days (e.g. 60)")
    parser.add_argument("--events-per-day", type=int, dest="events_per_day",
                        help="Override event volume per day (e.g. 20000)")
    args = parser.parse_args()

    out_dir = os.path.dirname(args.output)
    if out_dir:
        os.makedirs(out_dir, exist_ok=True)

    if args.reset and os.path.exists(args.output):
        os.remove(args.output)
        print(f"Removed {args.output}")

    conn = sqlite3.connect(args.output)
    create_schema(conn)

    existing = conn.execute("SELECT COUNT(*) FROM STAR_ACTION_AUDIT").fetchone()[0]
    if existing > 0:
        print(f"Database already contains {existing:,} rows. Use --reset to regenerate.")
        conn.close()
        sys.exit(0)

    params = dict(SCALE_PROFILES[args.scale])
    if args.users:          params["users"]          = args.users
    if args.days:           params["days"]           = args.days
    if args.events_per_day: params["events_per_day"] = args.events_per_day
    today  = date.today()
    start  = today - timedelta(days=params["days"])
    end    = today
    halfway = start + timedelta(days=params["days"] // 2)

    # UC-02: two problematic deploy dates — fixed to match v2.12.0 and v2.14.0 git tags
    # Must match BLAST_DAYS_AGO in mock_git_repo.py
    blast_dates = [today - timedelta(days=d) for d in (75, 35)]

    # UC-10: two anomalous dates
    anomaly_count_date    = today - timedelta(days=15)
    anomaly_duration_date = today - timedelta(days=7)

    normal_users, support_users, user_office, workstations = build_population(params)

    # Base feature pool (no New Blotter View — injected dynamically below)
    base_features = [f for f in FEATURES if f[1] != "New Blotter View"]
    base_names    = [f[1] for f in base_features]
    base_weights  = [f[2] for f in base_features]

    print(f"Generating '{args.scale}' dataset ({params['days']} days, ~{params['events_per_day']} events/day)...")

    total_inserted = 0
    batch = []
    BATCH_SIZE = 10_000

    current = start
    while current <= end:
        is_weekend = current.weekday() >= 5
        me_days    = last_two_business_days(current)

        # Day event count
        if current == anomaly_count_date:
            day_count = params["events_per_day"] * 5          # UC-10: ~4σ count spike
        elif current in me_days and not is_weekend:
            day_count = int(params["events_per_day"] * 3.0 * random.uniform(0.9, 1.1))  # UC-09
        elif is_weekend:
            day_count = params["events_per_day"] // 10
        else:
            day_count = int(params["events_per_day"] * random.uniform(0.8, 1.2))

        # Build effective feature pool for this date
        eff_names   = list(base_names)
        eff_weights = list(base_weights)

        # UC-05: remove zombies after halfway point
        if current > halfway:
            pairs = [(n, w) for n, w in zip(eff_names, eff_weights) if n not in ZOMBIE_FEATURES]
            if pairs:
                eff_names, eff_weights = map(list, zip(*pairs))

        # UC-08: S-curve adoption for New Blotter View
        nbv_w = nbv_adoption_weight(current, end)
        if nbv_w > 0.1:
            eff_names.append("New Blotter View")
            eff_weights.append(nbv_w)

        for _ in range(day_count):
            user   = random.choice(normal_users)
            office = user_office[user]
            ws     = random.choice(workstations[office])

            # UC-04: ~5 % impersonated support sessions
            if random.random() < 0.05:
                supp_user = random.choice(support_users)
                is_imp    = True
            else:
                supp_user = user
                is_imp    = False
            asmd_user = user

            feature      = weighted_choice(eff_names, eff_weights)
            ft           = FEATURE_MAP[feature]
            feature_type = ft[0]
            has_duration = ft[3]
            base_ms      = ft[4]
            stddev_ms    = ft[5]

            duration_ms = None
            if has_duration:
                d = sample_duration(base_ms, stddev_ms)
                d = int(d * degradation_factor(feature, current, end))
                d = int(d * blast_radius_factor(feature, current, blast_dates))
                d = int(d * lon_startup_factor(feature, office))
                d = int(d * impersonation_factor(is_imp))
                # UC-10: 5σ duration spike on anomaly day
                if current == anomaly_duration_date and feature in DEGRADING_FEATURES:
                    d = int(d * 5)
                duration_ms = max(1, d)

            ts     = make_timestamp(current, office)
            detail = make_detail(feature)

            batch.append((
                supp_user, asmd_user, ws, ts.isoformat(),
                feature_type, feature, detail, duration_ms,
            ))

            if len(batch) >= BATCH_SIZE:
                flush(conn, batch)
                total_inserted += len(batch)
                batch = []

        current += timedelta(days=1)

    if batch:
        flush(conn, batch)
        total_inserted += len(batch)

    print(f"\nMock Oracle DB: {args.output}")
    print(f"  STAR_ACTION_AUDIT: {total_inserted:,} rows")

    verify(conn, today, end, halfway, blast_dates, anomaly_count_date, anomaly_duration_date)

    conn.close()
