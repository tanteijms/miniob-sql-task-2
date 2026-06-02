"""Markdown / JSON test report generation."""

from __future__ import annotations

import json
from dataclasses import asdict, dataclass, field
from datetime import datetime, timezone
from typing import Any, Dict, List, Optional


@dataclass
class CaseResult:
    id: str
    name: str
    category: str
    feature: str
    points: int
    passed: bool
    duration_sec: float
    message: str = ""
    details: Dict[str, Any] = field(default_factory=dict)


@dataclass
class TestReport:
    started_at: str
    finished_at: str
    repo: str
    observer: str
    branch: str
    commit: str
    total: int = 0
    passed: int = 0
    failed: int = 0
    skipped: int = 0
    total_duration_sec: float = 0.0
    cases: List[CaseResult] = field(default_factory=list)

    def add(self, case: CaseResult) -> None:
        self.cases.append(case)
        self.total += 1
        if case.passed:
            self.passed += 1
        elif case.details.get("skipped"):
            self.skipped += 1
        else:
            self.failed += 1
        self.total_duration_sec += case.duration_sec


def render_markdown(report: TestReport) -> str:
    lines: List[str] = []
    lines.append("# MiniOB 全量测试报告")
    lines.append("")
    lines.append(f"- 开始时间：{report.started_at}")
    lines.append(f"- 结束时间：{report.finished_at}")
    lines.append(f"- 分支：`{report.branch}`  commit `{report.commit[:8] if report.commit else '?'}`")
    lines.append(f"- observer：`{report.observer}`")
    lines.append("")
    lines.append("## 汇总")
    lines.append("")
    lines.append("| 指标 | 数值 |")
    lines.append("|------|------|")
    lines.append(f"| 用例总数 | {report.total} |")
    lines.append(f"| 通过 | {report.passed} |")
    lines.append(f"| 失败 | {report.failed} |")
    lines.append(f"| 跳过 | {report.skipped} |")
    rate = (report.passed / report.total * 100) if report.total else 0
    lines.append(f"| 通过率 | {rate:.1f}% |")
    lines.append(f"| 总耗时 | {report.total_duration_sec:.1f}s |")
    lines.append("")

    by_category: Dict[str, List[CaseResult]] = {}
    for c in report.cases:
        by_category.setdefault(c.category, []).append(c)

    for category in ("official", "custom", "stress"):
        items = by_category.get(category, [])
        if not items:
            continue
        title = {"official": "官方用例", "custom": "自定义 SQL", "stress": "压力测试"}.get(
            category, category
        )
        lines.append(f"## {title}")
        lines.append("")
        lines.append("| 状态 | 用例 | 功能 | 分值 | 耗时 | 说明 |")
        lines.append("|------|------|------|------|------|------|")
        for c in items:
            status = "✅" if c.passed else ("⏭" if c.details.get("skipped") else "❌")
            msg = c.message.replace("|", "\\|")[:80]
            lines.append(
                f"| {status} | {c.id} | {c.feature} | {c.points} | {c.duration_sec:.1f}s | {msg} |"
            )
        lines.append("")

    failed = [c for c in report.cases if not c.passed and not c.details.get("skipped")]
    if failed:
        lines.append("## 失败详情")
        lines.append("")
        for c in failed:
            lines.append(f"### {c.id}")
            lines.append("")
            lines.append(c.message)
            if c.details:
                lines.append("")
                lines.append("```json")
                lines.append(json.dumps(c.details, ensure_ascii=False, indent=2))
                lines.append("```")
            lines.append("")

    return "\n".join(lines) + "\n"


def write_report(report: TestReport, out_dir: str) -> tuple[str, str]:
    import os

    os.makedirs(out_dir, exist_ok=True)
    ts = datetime.now().strftime("%Y%m%d-%H%M%S")
    md_path = os.path.join(out_dir, f"report-{ts}.md")
    json_path = os.path.join(out_dir, f"report-{ts}.json")
    latest_md = os.path.join(out_dir, "latest.md")
    latest_json = os.path.join(out_dir, "latest.json")

    md = render_markdown(report)
    payload = asdict(report)

    for path, content in (
        (md_path, md),
        (latest_md, md),
        (json_path, json.dumps(payload, ensure_ascii=False, indent=2)),
        (latest_json, json.dumps(payload, ensure_ascii=False, indent=2)),
    ):
        with open(path, "w", encoding="utf-8") as f:
            f.write(content)

    return md_path, json_path
