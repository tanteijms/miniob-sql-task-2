# 全量测试报告 — bupt-lab

> 时间：2026-06-03  
> 分支：`bupt-lab` @ `2de69eee`  
> 命令：`bash build.sh debug --make -j8` + `python3 lab/test/run_all.py`（**无 --quick**）

## 汇总

| 指标 | 数值 |
|------|------|
| 用例总数 | **35** |
| 通过 | **35** |
| 失败 | **0** |
| 总耗时 | **90.5s** |

详细 JSON/Markdown：`lab/test/report/report-20260603-142559.md`（`latest.md` 已指向该报告）

## 官方 primary 用例（14/14，manifest 内）

| 用例 | 功能 | 分值 | 结果 | 说明 |
|------|------|------|------|------|
| basic | basic | 2 | ✅ | expected_failure=0 |
| primary-drop-table | drop-table | 2 | ✅ | expected_failure=6 |
| primary-update | update | 2 | ✅ | expected_failure=4 |
| primary-date | date | 2 | ✅ | expected_failure=4 |
| primary-aggregation-func | aggregation | 2 | ✅ | expected_failure=7 |
| primary-join-tables | join | 2 | ✅ | expected_failure=0 |
| primary-order-by | order-by | 2 | ✅ | expected_failure=0 |
| primary-group-by | group-by | 4 | ✅ | expected_failure=0 |
| primary-multi-index | multi-index | 4 | ✅ | expected_failure=3 |
| primary-expression | function | 2 | ✅ | expected_failure=2 |
| primary-unique | unique | 2 | ✅ | expected_failure=2 |
| primary-simple-sub-query | simple-sub-query | 4 | ✅ | expected_failure=4 |
| primary-complex-sub-query | complex-sub-query | 5 | ✅ | expected_failure=4 |
| primary-null | null | 3 | ✅ | expected_failure=2 |

## 自定义补充用例（18）

覆盖 `lab/log/yys-dev` 各功能开发日志对应 SQL：

- basic / drop-table / update / update-index / date / date-delete  
- aggregation（含 count/avg 断言）/ like / join / function / order-by  
- group-by（含 HAVING 断言）/ multi-index / unique（含行数断言）  
- simple-sub-query / complex-sub-query / null / update-select  

全部 **PASS**（含预期 FAILURE 计数与 assertions 行集校验）。

## 压力测试（3）

| 用例 | 校验 | 结果 |
|------|------|------|
| big-write 500 | count(*)=420 + 重启持久化 | ✅ |
| big-write 2k | count(*)=1680 | ✅ |
| big-query 800 行 | count(*)=800，JOIN/GROUP/IN | ✅ |

## 仓库内未纳入本次回归的官方 .test（7）

课程/扩展题，当前 lab 未实现或未列入 manifest：

- `primary-text.test`
- `dblab-hash-join.test` / `dblab-optimizer.test` / `dblab-sort.test`
- `vectorized-basic.test` / `vectorized-aggregation-and-group-by.test` / `vectorized-order-by-limit.test`

## 结论

**bupt-lab 在「课程 primary + lab 自定义 + 压力」范围内全绿，可视为发布/合并就绪。**

复现：

```bash
bash build.sh debug --make -j8
python3 lab/test/run_all.py
```
