# SPDX-License-Identifier: Apache-2.0
# Copyright 2024-2026 LastState Contributors
# tools/latch_fleet_report.py
#
# Fleet report generator. Aggregates device health, versions,
# and anomaly trends.
#
# Heap-free, bounded, deterministic.

#!/usr/bin/env python3
"""Summarize a Latch SQLite fleet index and compare firmware crash signatures."""
from __future__ import annotations
import argparse, json, sqlite3
from collections import defaultdict
from pathlib import Path


def compare_builds(db: sqlite3.Connection, baseline: str, candidate: str) -> dict[str, object]:
    def signatures(build: str) -> dict[int, tuple[int, int]]:
        rows = db.execute(
            """SELECT fingerprint, COUNT(*), COUNT(DISTINCT device_id) FROM events
               WHERE COALESCE(build_id,'unknown')=? AND fingerprint IS NOT NULL AND fingerprint != 0
               GROUP BY fingerprint""", (build,)).fetchall()
        return {int(fp): (int(events), int(devices)) for fp, events, devices in rows}
    base, cand = signatures(baseline), signatures(candidate)
    new = sorted(set(cand) - set(base))
    resolved = sorted(set(base) - set(cand))
    shared = sorted(set(base) & set(cand))
    return {
        "baseline": baseline,
        "candidate": candidate,
        "new_signatures": [{"fingerprint": fp, "events": cand[fp][0], "devices": cand[fp][1]} for fp in new],
        "resolved_signatures": [{"fingerprint": fp, "events": base[fp][0], "devices": base[fp][1]} for fp in resolved],
        "shared_signatures": [
            {"fingerprint": fp, "baseline_events": base[fp][0], "candidate_events": cand[fp][0],
             "baseline_devices": base[fp][1], "candidate_devices": cand[fp][1]}
            for fp in shared
        ],
    }



def evaluate_canary(db: sqlite3.Connection, baseline: str, candidate: str, *,
                    minimum_candidate_devices: int = 5,
                    maximum_new_crash_signatures: int = 0,
                    maximum_crash_device_rate_delta: float = 0.02) -> dict[str, object]:
    """Return a conservative promotion decision from persisted fleet evidence.

    This is an operational guardrail, not a safety certification. Rates use the
    distinct devices observed on each build as the denominator and devices with
    at least one crash event (LEP event_type=1) as the numerator.
    """
    if minimum_candidate_devices < 1 or maximum_new_crash_signatures < 0 or maximum_crash_device_rate_delta < 0:
        raise ValueError("invalid canary thresholds")

    def stats(build: str) -> tuple[int, int, set[int]]:
        devices = int(db.execute(
            "SELECT COUNT(DISTINCT device_id) FROM events WHERE COALESCE(build_id,'unknown')=?",
            (build,),
        ).fetchone()[0])
        crash_devices = int(db.execute(
            """SELECT COUNT(DISTINCT device_id) FROM events
               WHERE COALESCE(build_id,'unknown')=? AND event_type=1""", (build,),
        ).fetchone()[0])
        signatures = {int(row[0]) for row in db.execute(
            """SELECT DISTINCT fingerprint FROM events
               WHERE COALESCE(build_id,'unknown')=? AND event_type=1
                 AND fingerprint IS NOT NULL AND fingerprint != 0""", (build,))}
        return devices, crash_devices, signatures

    base_devices, base_crash_devices, base_signatures = stats(baseline)
    cand_devices, cand_crash_devices, cand_signatures = stats(candidate)
    base_rate = (base_crash_devices / base_devices) if base_devices else 0.0
    cand_rate = (cand_crash_devices / cand_devices) if cand_devices else 0.0
    new_signatures = sorted(cand_signatures - base_signatures)
    reasons: list[str] = []
    if cand_devices < minimum_candidate_devices:
        reasons.append(f"candidate sample too small: {cand_devices} < {minimum_candidate_devices} devices")
    if len(new_signatures) > maximum_new_crash_signatures:
        reasons.append(
            f"new crash signatures: {len(new_signatures)} > {maximum_new_crash_signatures}")
    delta = cand_rate - base_rate
    if delta > maximum_crash_device_rate_delta:
        reasons.append(
            f"crash-device rate delta {delta:.6f} > {maximum_crash_device_rate_delta:.6f}")
    return {
        "baseline": baseline, "candidate": candidate,
        "baseline_devices": base_devices, "candidate_devices": cand_devices,
        "baseline_crash_devices": base_crash_devices, "candidate_crash_devices": cand_crash_devices,
        "baseline_crash_device_rate": base_rate, "candidate_crash_device_rate": cand_rate,
        "crash_device_rate_delta": delta, "new_crash_signatures": new_signatures,
        "thresholds": {"minimum_candidate_devices": minimum_candidate_devices,
                       "maximum_new_crash_signatures": maximum_new_crash_signatures,
                       "maximum_crash_device_rate_delta": maximum_crash_device_rate_delta},
        "promote": not reasons, "reasons": reasons,
    }

def build_report(path: Path, baseline: str | None = None, candidate: str | None = None, *,
                 canary: bool = False, minimum_candidate_devices: int = 5,
                 maximum_new_crash_signatures: int = 0,
                 maximum_crash_device_rate_delta: float = 0.02) -> dict[str, object]:
    db = sqlite3.connect(path)
    try:
        builds = [dict(zip(("build_id", "events", "devices", "crash_signatures", "first_seen_ns", "last_seen_ns"), row)) for row in db.execute(
            """SELECT COALESCE(build_id,'unknown'), COUNT(*), COUNT(DISTINCT device_id),
                      COUNT(DISTINCT CASE WHEN fingerprint IS NOT NULL AND fingerprint != 0 THEN fingerprint END),
                      MIN(received_ns), MAX(received_ns)
               FROM events GROUP BY COALESCE(build_id,'unknown') ORDER BY MIN(received_ns), COALESCE(build_id,'unknown')""")]
        clusters = [dict(zip(("fingerprint", "build_id", "events", "devices", "first_seen_ns", "last_seen_ns"), row)) for row in db.execute(
            """SELECT fingerprint, COALESCE(build_id,'unknown'), COUNT(*), COUNT(DISTINCT device_id),
                      MIN(received_ns), MAX(received_ns) FROM events
               WHERE fingerprint IS NOT NULL AND fingerprint != 0
               GROUP BY fingerprint, COALESCE(build_id,'unknown') ORDER BY COUNT(*) DESC""")]

        # Determine the actual first-seen build by timestamp, not lexical build-id order.
        first_seen: dict[int, tuple[str, int]] = {}
        build_sets: dict[int, set[str]] = defaultdict(set)
        counts: dict[int, int] = defaultdict(int)
        for fp, build_id, received_ns in db.execute(
            """SELECT fingerprint, COALESCE(build_id,'unknown'), received_ns FROM events
               WHERE fingerprint IS NOT NULL AND fingerprint != 0
               ORDER BY received_ns, rowid"""):
            fp = int(fp); build_id = str(build_id); received_ns = int(received_ns)
            first_seen.setdefault(fp, (build_id, received_ns))
            build_sets[fp].add(build_id); counts[fp] += 1
        regressions = [
            {"fingerprint": fp, "first_build": first_seen[fp][0], "first_seen_ns": first_seen[fp][1],
             "build_count": len(build_sets[fp]), "events": counts[fp]}
            for fp in sorted(first_seen, key=lambda x: (-counts[x], x)) if len(build_sets[fp]) == 1
        ]
        missions = [dict(zip(("mission_id", "dives", "events", "devices"), row)) for row in db.execute(
            """SELECT mission_id, COUNT(DISTINCT dive_id), COUNT(*), COUNT(DISTINCT device_id)
               FROM events WHERE mission_id IS NOT NULL GROUP BY mission_id ORDER BY COUNT(*) DESC""")]
        report: dict[str, object] = {"builds": builds, "crash_clusters": clusters,
                                    "single_build_signatures": regressions, "missions": missions}
        if baseline is not None and candidate is not None:
            report["build_comparison"] = compare_builds(db, baseline, candidate)
            if canary:
                report["canary_decision"] = evaluate_canary(
                    db, baseline, candidate, minimum_candidate_devices=minimum_candidate_devices,
                    maximum_new_crash_signatures=maximum_new_crash_signatures,
                    maximum_crash_device_rate_delta=maximum_crash_device_rate_delta)
        return report
    finally:
        db.close()


def main() -> int:
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument("--index", required=True, type=Path)
    parser.add_argument("--output", type=Path)
    parser.add_argument("--baseline-build")
    parser.add_argument("--candidate-build")
    parser.add_argument("--canary", action="store_true", help="evaluate conservative canary promotion gates")
    parser.add_argument("--minimum-candidate-devices", type=int, default=5)
    parser.add_argument("--maximum-new-crash-signatures", type=int, default=0)
    parser.add_argument("--maximum-crash-device-rate-delta", type=float, default=0.02)
    args = parser.parse_args()
    if (args.baseline_build is None) != (args.candidate_build is None):
        parser.error("--baseline-build and --candidate-build must be supplied together")
    if args.canary and (args.baseline_build is None or args.candidate_build is None):
        parser.error("--canary requires --baseline-build and --candidate-build")
    report = json.dumps(build_report(
        args.index, args.baseline_build, args.candidate_build, canary=args.canary,
        minimum_candidate_devices=args.minimum_candidate_devices,
        maximum_new_crash_signatures=args.maximum_new_crash_signatures,
        maximum_crash_device_rate_delta=args.maximum_crash_device_rate_delta),
        indent=2, sort_keys=True)
    if args.output:
        args.output.write_text(report + "\n", encoding="utf-8")
    else:
        print(report)
    return 0
if __name__ == "__main__":
    raise SystemExit(main())
