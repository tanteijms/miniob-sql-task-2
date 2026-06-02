# MiniOB 全量 / 压力测试

对已开发功能做**官方用例 + 自定义 SQL + 压力脚本**三层回归，并生成 Markdown / JSON 测试报告。

## 覆盖范围

| 类别 | 内容 |
|------|------|
| 官方用例 | `test/case/test/*.test`（basic、drop-table、update、date、aggregation、join、order-by、group-by、multi-index、expression） |
| 自定义 SQL | `lab/test/cases/sql/*.sql`（含 HAVING、索引 DML 等补充场景） |
| 压力测试 | `lab/test/stress/gen_big_write_sql.py`（500 行 + 重启校验）、`gen_big_write_2k.py`（2000 行，标记 slow） |

## 快速开始

```bash
# 仓库根目录
bash build.sh debug --make -j8
bash lab/test/run_all.sh
```

常用参数：

```bash
bash lab/test/run_all.sh --quick          # 跳过 2k 压力（约 1 分钟内跑完主体）
bash lab/test/run_all.sh --build          # 测试前先编译
bash lab/test/run_all.sh --only custom-group-by,stress-big-write
python3 lab/test/run_all.py --help
```

## 报告输出

运行后在 `lab/test/report/` 生成：

- `latest.md` / `latest.json` — 最近一次结果
- `report-YYYYMMDD-HHMMSS.md` — 带时间戳归档

报告包含：通过率、各用例耗时、失败详情（含 FAILURE 计数、断言 diff、count(*) 校验等）。

## 判定规则

1. **官方 / 自定义 SQL**：observer 正常退出，且 `FAILURE` 行数等于 manifest 中 `expected_failure`（含故意测错的语句）。
2. **自定义断言**：主 SQL 跑完后在同一库上执行 `assertions`，比对查询结果行。
3. **压力测试**：解析 `SELECT count(*)`，与 generator stderr 中的 `expected_rows` 一致；`big-write` 额外重启 observer 验证持久化。

## 目录结构

```text
lab/test/
  manifest.json       # 用例清单与期望基线
  run_all.py / .sh    # 入口
  lib/                # runner + reporter
  cases/sql/          # 自定义 SQL
  stress/             # 压力脚本
  report/             # 输出（gitignore）
```

## 扩展

新增功能时：

1. 在 `cases/sql/` 增加 SQL（可选 `assertions`）
2. 在 `manifest.json` 增加条目（填好 `expected_failure` 基线）
3. 跑一遍 `bash lab/test/run_all.sh`，确认报告全绿

修改 `expected_failure` 前请确认是**预期行为变化**而非回归。
