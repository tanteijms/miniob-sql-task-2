#!/usr/bin/env python3
"""
MiniOB 全量回归 + 压力测试入口。

用法（在仓库根目录）:
  python3 lab/test/run_all.py
  python3 lab/test/run_all.py --quick          # 跳过 stress/slow
  python3 lab/test/run_all.py --build          # 测试前先编译
  python3 lab/test/run_all.py --only official-basic,custom-group-by
"""

from __future__ import annotations

import argparse
import json
import os
import subprocess
import sys
import time
from datetime import datetime, timezone
from typing import Any, Dict, List, Optional

LAB_TEST_DIR = os.path.dirname(os.path.abspath(__file__))
REPO_ROOT = os.path.abspath(os.path.join(LAB_TEST_DIR, "../.."))
sys.path.insert(0, LAB_TEST_DIR)

from lib.reporter import CaseResult, TestReport, write_report  # noqa: E402
from lib.comprehensive import run_comprehensive_suite  # noqa: E402
from lib.runner import (  # noqa: E402
    MiniOBCliRunner,
    default_db_dir,
    find_observer,
    parse_last_scalar,
    parse_official_test_commands,
    rows_match,
    run_generator,
)


def git_info(repo: str) -> tuple[str, str]:
    try:
        branch = subprocess.check_output(
            ["git", "rev-parse", "--abbrev-ref", "HEAD"], cwd=repo, text=True
        ).strip()
        commit = subprocess.check_output(
            ["git", "rev-parse", "HEAD"], cwd=repo, text=True
        ).strip()
        return branch, commit
    except Exception:
        return "unknown", "unknown"


def load_manifest(path: str) -> dict:
    with open(path, encoding="utf-8") as f:
        return json.load(f)


def resolve_path(rel: str) -> str:
    if os.path.isabs(rel):
        return rel
    return os.path.join(REPO_ROOT, rel)


def run_official_suite(runner: MiniOBCliRunner, suite: dict) -> CaseResult:
    path = resolve_path(suite["file"])
    timeout = suite.get("timeout_sec", 60)
    expected_failure = suite.get("expected_failure", 0)

    with open(path, encoding="utf-8") as f:
        commands = parse_official_test_commands(f.readlines())

    session = runner.run_commands(commands, clean_db=True, timeout=timeout)
    passed = (
        session.exit_code == 0
        and session.failure_count == expected_failure
    )
    msg = (
        f"FAILURE {session.failure_count} (expected {expected_failure}), "
        f"SUCCESS {session.success_count}, exit={session.exit_code}"
    )
    if session.exit_code == 124:
        passed = False
        msg = "timeout"

    return CaseResult(
        id=suite["id"],
        name=suite.get("name", suite["id"]),
        category=suite["category"],
        feature=suite["feature"],
        points=suite.get("points", 0),
        passed=passed,
        duration_sec=session.duration_sec,
        message=msg if not passed else "ok",
        details={
            "success": session.success_count,
            "failure": session.failure_count,
            "expected_failure": expected_failure,
            "commands": len(commands),
            "exit_code": session.exit_code,
        },
    )


def run_custom_suite(runner: MiniOBCliRunner, suite: dict) -> CaseResult:
    path = resolve_path(suite["file"])
    timeout = suite.get("timeout_sec", 60)
    expected_failure = suite.get("expected_failure", 0)

    session = runner.run_sql_file(path, clean_db=True, timeout=timeout)
    passed = session.exit_code == 0 and session.failure_count == expected_failure
    messages: List[str] = []

    if session.exit_code == 124:
        passed = False
        messages.append("timeout")
    elif session.failure_count != expected_failure:
        passed = False
        messages.append(
            f"FAILURE {session.failure_count} != expected {expected_failure}"
        )

    # assertions reuse same DB (no clean)
    for assertion in suite.get("assertions", []):
        sql = assertion["sql"]
        qr = runner.run_sql(sql, clean_db=False, timeout=timeout)
        exp_rows = assertion.get("rows", [])
        sort = assertion.get("sort", False)
        ok, diff = rows_match(qr.rows, exp_rows, sort=sort)
        if not ok:
            passed = False
            messages.append(f"{assertion.get('name', sql)}: {diff}")

    return CaseResult(
        id=suite["id"],
        name=suite.get("name", suite["id"]),
        category=suite["category"],
        feature=suite["feature"],
        points=suite.get("points", 0),
        passed=passed,
        duration_sec=session.duration_sec,
        message="; ".join(messages) if messages else "ok",
        details={
            "success": session.success_count,
            "failure": session.failure_count,
            "expected_failure": expected_failure,
            "assertions": len(suite.get("assertions", [])),
        },
    )


def run_stress_suite(runner: MiniOBCliRunner, suite: dict) -> CaseResult:
    gen_path = resolve_path(suite["generator"])
    timeout = suite.get("timeout_sec", 120)
    expected_rows = suite.get("expected_rows")

    commands, gen_expected = run_generator(gen_path)
    if gen_expected is not None:
        expected_rows = gen_expected

    session = runner.run_commands(commands, clean_db=True, timeout=timeout)
    count_val = parse_last_scalar(session.stdout, "count(*)")
    passed = session.exit_code == 0 and session.failure_count == 0
    messages: List[str] = []

    if count_val is None:
        passed = False
        messages.append("cannot parse count(*)")
    elif expected_rows is not None and str(count_val) != str(expected_rows):
        passed = False
        messages.append(f"count(*)={count_val}, expected {expected_rows}")

    restart_count = None
    if suite.get("restart_check") and passed:
        # fresh process, same on-disk data
        restart = runner.run_commands(
            ["SELECT count(*) FROM big_write;"],
            clean_db=False,
            timeout=30,
        )
        restart_count = parse_last_scalar(restart.stdout, "count(*)")
        if restart_count != count_val:
            passed = False
            messages.append(
                f"restart count(*)={restart_count}, expected {count_val}"
            )

    return CaseResult(
        id=suite["id"],
        name=suite.get("name", suite["id"]),
        category=suite["category"],
        feature=suite["feature"],
        points=suite.get("points", 0),
        passed=passed,
        duration_sec=session.duration_sec,
        message="; ".join(messages) if messages else f"count(*)={count_val}",
        details={
            "count": count_val,
            "expected_rows": expected_rows,
            "restart_count": restart_count,
            "failure": session.failure_count,
            "statements": len(commands),
        },
    )


def maybe_build(repo: str) -> None:
    print("==> building (debug)...", flush=True)
    subprocess.check_call(
        ["bash", "build.sh", "debug", "--make", "-j8"],
        cwd=repo,
    )


def filter_suites(
    suites: List[dict], only: Optional[List[str]], quick: bool
) -> List[dict]:
    out = suites
    if quick:
        out = [s for s in out if "slow" not in s.get("tags", [])]
    if only:
        only_set = set(only)
        out = [s for s in out if s["id"] in only_set]
    return out


def main() -> int:
    parser = argparse.ArgumentParser(description="MiniOB full regression + stress tests")
    parser.add_argument(
        "--manifest",
        default=os.path.join(LAB_TEST_DIR, "manifest.json"),
        help="test manifest path",
    )
    parser.add_argument(
        "--report-dir",
        default=os.path.join(LAB_TEST_DIR, "report"),
        help="directory for markdown/json reports",
    )
    parser.add_argument("--build", action="store_true", help="run build.sh debug before tests")
    parser.add_argument("--quick", action="store_true", help="skip slow stress cases")
    parser.add_argument(
        "--only",
        help="comma-separated suite ids to run",
    )
    args = parser.parse_args()

    if args.build:
        maybe_build(REPO_ROOT)

    observer = find_observer(REPO_ROOT)
    config = os.path.join(REPO_ROOT, "etc", "observer.ini")
    db_dir = default_db_dir(observer)
    runner = MiniOBCliRunner(REPO_ROOT, observer, config, db_dir)

    manifest = load_manifest(args.manifest)
    suites = filter_suites(
        manifest["suites"],
        args.only.split(",") if args.only else None,
        args.quick,
    )

    branch, commit = git_info(REPO_ROOT)
    started = datetime.now(timezone.utc).astimezone().isoformat(timespec="seconds")
    t0 = time.time()

    report = TestReport(
        started_at=started,
        finished_at="",
        repo=REPO_ROOT,
        observer=observer,
        branch=branch,
        commit=commit,
    )

    print(f"==> observer: {observer}")
    print(f"==> running {len(suites)} suite(s)...")

    for i, suite in enumerate(suites, 1):
        cat = suite["category"]
        sid = suite["id"]
        print(f"[{i}/{len(suites)}] {sid} ({cat})...", flush=True)

        if cat == "official":
            result = run_official_suite(runner, suite)
        elif cat == "custom":
            result = run_custom_suite(runner, suite)
        elif cat == "stress":
            result = run_stress_suite(runner, suite)
        elif cat == "comprehensive":
            sub_results = run_comprehensive_suite(runner, suite, resolve_path)
            for result in sub_results:
                report.add(result)
            passed_n = sum(1 for r in sub_results if r.passed)
            print(
                f"    -> {'PASS' if passed_n == len(sub_results) else 'FAIL'} "
                f"({sum(r.duration_sec for r in sub_results):.1f}s) "
                f"{passed_n}/{len(sub_results)} cases",
                flush=True,
            )
            continue
        else:
            result = CaseResult(
                id=sid,
                name=suite.get("name", sid),
                category=cat,
                feature=suite.get("feature", "?"),
                points=suite.get("points", 0),
                passed=False,
                duration_sec=0,
                message=f"unknown category: {cat}",
            )

        if cat != "comprehensive":
            report.add(result)
            mark = "PASS" if result.passed else "FAIL"
            print(f"    -> {mark} ({result.duration_sec:.1f}s) {result.message}")

    report.finished_at = (
        datetime.now(timezone.utc).astimezone().isoformat(timespec="seconds")
    )
    report.total_duration_sec = time.time() - t0

    md_path, json_path = write_report(report, args.report_dir)
    print("")
    print(f"==> done: {report.passed}/{report.total} passed in {report.total_duration_sec:.1f}s")
    print(f"==> report: {md_path}")
    print(f"==> json:   {json_path}")

    return 0 if report.failed == 0 else 1


if __name__ == "__main__":
    raise SystemExit(main())
