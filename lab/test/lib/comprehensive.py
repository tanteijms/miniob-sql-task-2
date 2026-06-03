"""Run per-case assertion suites from comprehensive/cases.json."""

from __future__ import annotations

import json
import time
from typing import List, Tuple

from lib.reporter import CaseResult
from lib.runner import MiniOBCliRunner, rows_match


def expand_commands(commands: List[str]) -> List[str]:
    """Split ';'-joined setup lines into individual CLI statements."""
    out: List[str] = []
    for cmd in commands:
        for part in cmd.split(";"):
            part = part.strip()
            if part:
                out.append(part)
    return out


def load_cases(path: str) -> List[dict]:
    with open(path, encoding="utf-8") as f:
        data = json.load(f)
    return data["cases"]


def run_one_case(runner: MiniOBCliRunner, case: dict, timeout: int) -> Tuple[bool, str, float]:
    t0 = time.time()
    cid = case.get("id", "?")
    runner.reset_db()

    setup = expand_commands(case.get("setup") or [])
    sql = case["sql"]

    if case.get("expect_failure"):
        session = runner.run_commands(setup + [sql], clean_db=False, timeout=timeout)
        if session.exit_code == 124:
            return False, "timeout", time.time() - t0
        if session.exit_code != 0 and session.failure_count == 0:
            return True, "ok (process error on expected failure)", time.time() - t0
        if session.failure_count >= 1:
            return True, "ok", time.time() - t0
        return False, "expected FAILURE got SUCCESS", time.time() - t0

    if setup:
        session = runner.run_commands(setup, clean_db=False, timeout=timeout)
        if session.exit_code == 124:
            return False, "setup timeout", time.time() - t0
        exp_setup_fail = case.get("setup_expected_failure", 0)
        if session.failure_count > exp_setup_fail:
            return (
                False,
                f"setup FAILURE {session.failure_count} (expected {exp_setup_fail})",
                time.time() - t0,
            )

    qr = runner.run_sql(sql, clean_db=False, timeout=timeout)
    if not qr.ok and not qr.rows:
        return False, f"query failed or empty output for {cid}", time.time() - t0

    expected = case.get("rows", [])
    ok, diff = rows_match(qr.rows, expected, sort=case.get("sort", False))
    if ok:
        return True, "ok", time.time() - t0
    return False, diff, time.time() - t0


def run_comprehensive_suite(runner: MiniOBCliRunner, suite: dict, resolve_path) -> List[CaseResult]:
    path = resolve_path(suite["file"])
    timeout = suite.get("case_timeout_sec", 20)
    cases = load_cases(path)
    results: List[CaseResult] = []

    for case in cases:
        passed, msg, dur = run_one_case(runner, case, timeout)
        results.append(
            CaseResult(
                id=case.get("id", "comp-?"),
                name=case.get("name", case.get("id", "?")),
                category="comprehensive",
                feature=case.get("feature", "integration"),
                points=0,
                passed=passed,
                duration_sec=dur,
                message=msg,
                details={"sql": case.get("sql", "")[:120]},
            )
        )
    return results
