"""MiniOB CLI test runner and result parsing."""

from __future__ import annotations

import os
import re
import subprocess
import time
from dataclasses import dataclass, field
from typing import Iterable, List, Optional, Sequence, Tuple


@dataclass
class SqlResult:
    sql: str
    ok: bool
    raw: str = ""
    columns: List[str] = field(default_factory=list)
    rows: List[List[str]] = field(default_factory=list)


@dataclass
class SessionResult:
    exit_code: int
    stdout: str
    stderr: str
    duration_sec: float
    success_count: int = 0
    failure_count: int = 0
    results: List[SqlResult] = field(default_factory=list)


class MiniOBCliRunner:
    BANNER_END = "Successfully load"

    def __init__(
        self,
        repo_root: str,
        observer: str,
        config: str,
        db_dir: str,
        default_timeout: int = 120,
    ):
        self.repo_root = repo_root
        self.observer = observer
        self.config = config
        self.db_dir = db_dir
        self.default_timeout = default_timeout
        self.observer_cwd = os.path.dirname(observer)

    def reset_db(self) -> None:
        import shutil

        shutil.rmtree(self.db_dir, ignore_errors=True)
        os.makedirs(self.db_dir, exist_ok=True)

    def run_commands(
        self,
        commands: Sequence[str],
        *,
        clean_db: bool = True,
        timeout: Optional[int] = None,
    ) -> SessionResult:
        if clean_db:
            self.reset_db()

        body = [c.strip() for c in commands if c.strip()]
        body.append("exit")
        sql_input = "\n".join(body) + "\n"

        t0 = time.time()
        try:
            proc = subprocess.run(
                [self.observer, "-f", self.config, "-P", "cli"],
                input=sql_input,
                text=True,
                capture_output=True,
                cwd=self.observer_cwd,
                timeout=timeout or self.default_timeout,
            )
        except subprocess.TimeoutExpired as ex:
            duration = time.time() - t0
            return SessionResult(
                exit_code=124,
                stdout=ex.stdout or "",
                stderr=ex.stderr or "",
                duration_sec=duration,
            )

        duration = time.time() - t0
        stdout = proc.stdout or ""
        session = SessionResult(
            exit_code=proc.returncode,
            stdout=stdout,
            stderr=proc.stderr or "",
            duration_sec=duration,
        )
        session.success_count = len(re.findall(r"^SUCCESS$", stdout, re.M))
        session.failure_count = len(re.findall(r"^FAILURE$", stdout, re.M))
        return session

    def run_sql(self, sql: str, *, clean_db: bool = False, timeout: Optional[int] = None) -> SqlResult:
        session = self.run_commands([sql], clean_db=clean_db, timeout=timeout)
        parsed = parse_query_output(session.stdout, sql)
        if session.failure_count > 0:
            parsed.ok = False
        elif parsed.columns or parsed.rows:
            parsed.ok = True
        elif session.success_count >= 1:
            parsed.ok = True
        else:
            parsed.ok = session.exit_code == 0 and session.failure_count == 0
        return parsed

    def run_sql_file(self, path: str, *, clean_db: bool = True, timeout: Optional[int] = None) -> SessionResult:
        with open(path, encoding="utf-8") as f:
            commands = extract_sql_commands(f.readlines())
        return self.run_commands(commands, clean_db=clean_db, timeout=timeout)


def find_observer(repo_root: str) -> str:
    candidates = [
        os.path.join(repo_root, "build_debug", "bin", "observer"),
        os.path.join(repo_root, "build", "bin", "observer"),
    ]
    for path in candidates:
        if os.path.isfile(path) and os.access(path, os.X_OK):
            return path
    raise FileNotFoundError(
        "observer not found. Run: bash build.sh debug --make -j8"
    )


def default_db_dir(observer: str) -> str:
    return os.path.join(os.path.dirname(observer), "miniob", "db", "sys")


def extract_sql_commands(lines: Iterable[str]) -> List[str]:
    """Parse plain .sql files: strip blank lines and -- comments."""
    commands: List[str] = []
    for line in lines:
        stripped = line.strip()
        if not stripped or stripped.startswith("#"):
            continue
        if stripped.startswith("--"):
            continue
        commands.append(stripped)
    return commands


def parse_official_test_commands(lines: Iterable[str]) -> List[str]:
    """Parse official .test files (miniob_test dialect)."""
    commands: List[str] = []
    for line in lines:
        stripped = line.strip()
        if not stripped or stripped.startswith("#"):
            continue
        if stripped.startswith("--"):
            rest = stripped[2:].strip()
            if rest.startswith("sort "):
                commands.append(rest[5:].strip())
            elif rest.startswith("echo") or rest.startswith("ensure"):
                continue
            continue
        commands.append(stripped)
    return commands


def parse_query_output(stdout: str, sql: str) -> SqlResult:
    """Extract the last query result block from CLI stdout."""
    result = SqlResult(sql=sql, ok=False)
    marker = "Successfully load"
    start = stdout.find(marker)
    if start < 0:
        return result
    body = stdout[start:].split("\n")
    # Walk backwards to find last result set (header + rows before trailing SUCCESS/FAILURE/empty)
    idx = len(body) - 1
    while idx >= 0 and body[idx].strip() == "":
        idx -= 1
    if idx < 0:
        return result

    row_lines: List[str] = []
    while idx >= 0 and body[idx].strip() not in ("SUCCESS", "FAILURE") and not body[idx].startswith("Successfully load"):
        if body[idx].strip():
            row_lines.insert(0, body[idx].strip())
        idx -= 1

    if not row_lines:
        return result

    header = row_lines[0]
    result.columns = [c.strip() for c in header.split("|")] if "|" in header else [header]
    if len(row_lines) > 1:
        for line in row_lines[1:]:
            if "|" in line:
                result.rows.append([c.strip() for c in line.split("|")])
            else:
                result.rows.append([line.strip()])
    result.ok = True
    return result


def parse_last_scalar(stdout: str, column: str = "") -> Optional[str]:
    """Return the single cell from the last SELECT result."""
    start = stdout.find("Successfully load")
    if start < 0:
        return None
    lines = [ln.strip() for ln in stdout[start:].split("\n") if ln.strip()]
    # find last header block
    for i in range(len(lines) - 1, -1, -1):
        if lines[i] in ("SUCCESS", "FAILURE"):
            continue
        if i + 1 < len(lines) and lines[i + 1] not in ("SUCCESS", "FAILURE"):
            # lines[i] is header, lines[i+1] is value row (maybe single column)
            if column and lines[i] != column:
                continue
            val_line = lines[i + 1]
            if "|" in val_line:
                return val_line.split("|")[0].strip()
            return val_line
    return None


def rows_match(
    actual: Sequence[Sequence[str]],
    expected: Sequence[Sequence[str]],
    *,
    float_tol: float = 1e-3,
    sort: bool = False,
) -> Tuple[bool, str]:
    def norm_row(row: Sequence[str]) -> Tuple[str, ...]:
        out = []
        for cell in row:
            cell = cell.strip()
            try:
                f = float(cell)
                out.append(f"{f:.4f}".rstrip("0").rstrip("."))
            except ValueError:
                out.append(cell)
        return tuple(out)

    act = [norm_row(r) for r in actual]
    exp = [norm_row(r) for r in expected]
    if sort:
        act = sorted(act)
        exp = sorted(exp)
    if act == exp:
        return True, ""
    return False, f"expected {list(exp)}, got {list(act)}"


def run_generator(path: str) -> Tuple[List[str], Optional[int]]:
    import sys

    proc = subprocess.run(
        [sys.executable, path],
        text=True,
        capture_output=True,
        check=False,
    )
    expected_rows = None
    m = re.search(r"expected_rows=(\d+)", proc.stderr)
    if m:
        expected_rows = int(m.group(1))
    commands = extract_sql_commands(proc.stdout.splitlines())
    return commands, expected_rows
