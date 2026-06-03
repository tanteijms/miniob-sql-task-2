# 开发日志：big-query

> 记录时间：2026-06-03
> 工作区：XC
> 任务定位：`7) big-query` 大数据量查询稳定性与压力验证收口
> 当前状态：压力测试资产已接管并补齐少量边界覆盖，功能主链待同伴编译/调试/压力测试对接

## 任务范围

- 本题不新增 SQL 语法能力，不修改 parser / AST / stmt 主链。
- 本题不主动引入 hash join、索引选择优化、外部排序、分组哈希表重构等大改。
- 当前验收入口以 `lab/test/manifest.json` 中的 `stress-big-query` 为准。
- 参考日志为 `lab/log/yys-dev/19开发日志-big-query.md`，本轮不覆盖 yys 工作区日志。

## 已确认资产

- `lab/test/stress/gen_big_query_sql.py` 已存在确定性压力生成器。
- 生成规模保持不变：主表 `bq_main` 800 行，维表 `bq_dim` 20 行，固定 `SEED = 7`。
- manifest 中 `stress-big-query` 保持：
  - `generator = lab/test/stress/gen_big_query_sql.py`
  - `expected_rows = 800`
  - `timeout_sec = 90`
  - `tags = ["slow"]`
- `run_all.py` 对 stress 用例会解析输出中最后一次 `count(*)` 标量，因此最终 `SELECT count(*) FROM bq_main;` 必须保持在脚本最后。

## 压力覆盖面

当前生成器覆盖：

- 大表按主键点查：`SELECT id, v FROM bq_main WHERE id = ...`
- 索引过滤计数：`SELECT count(*) FROM bq_main WHERE k = ...`
- 聚合分组：`SELECT k, count(*), sum(v) FROM bq_main GROUP BY k`
- HAVING：`SELECT k, avg(v) FROM bq_main GROUP BY k HAVING count(*) > 30`
- IN 子查询与排序：`SELECT id FROM bq_main WHERE k IN (SELECT k FROM bq_dim WHERE label > 500) ORDER BY id`
- 二表 join：`SELECT count(*) FROM bq_main, bq_dim WHERE bq_main.k = bq_dim.k`
- 随机点查：固定 seed 下 15 次 `SELECT v FROM bq_main WHERE id = ...`
- 最终验收计数：`SELECT count(*) FROM bq_main`，期望为 800。

## 本轮补充

已在 `lab/test/stress/gen_big_query_sql.py` 增加少量低风险边界查询：

- 空结果过滤：`SELECT count(*) FROM bq_main WHERE id=99999;`
- join 后附加维表过滤：`SELECT count(*) FROM bq_main, bq_dim WHERE bq_main.k = bq_dim.k AND bq_dim.label = 700;`
- 小范围 order by：`SELECT id FROM bq_main WHERE k = 3 ORDER BY id;`

这些查询均放在最终 `SELECT count(*) FROM bq_main;` 之前，不改变 `expected_rows=800` 的最终验收口径，也避免构造三表大笛卡尔积或巨大结果输出。

## 静态复核结论

已按计划只读复核压力相关文件入口：

- `src/observer/sql/operator/table_scan_physical_operator.*`
- `src/observer/sql/operator/sort_physical_operator.*`
- `src/observer/sql/operator/hash_group_by_physical_operator.*`
- `src/observer/sql/operator/nested_loop_join_physical_operator.*`
- `src/observer/sql/operator/hash_join_physical_operator.*`
- 子查询表达式与前置 simple-sub-query / update-select 主链日志

当前未发现必须在 `src/` 中立即点状修复的明确缺陷。本题先按“压力资产补齐 + 日志接管”交付；若同伴后续给出编译、超时、结果错误或资源释放失败日志，再回到对应算子做最小修复。

## 待同伴验证

本轮按团队最新协定不执行本地编译和测试。建议同伴后续运行：

```bash
python3 lab/test/run_all.py --only stress-big-query
python3 lab/test/run_all.py --quick
```

失败分类与处理建议：

- 编译失败：记录具体文件和符号，不扩大到算法重构。
- 超时：先检查压力脚本是否输出过大结果，再排查算子重复 open/materialize 或无限循环。
- 结果错误：回溯到前置功能点，如 group-by、join、simple-sub-query、order-by。
- 内存或资源问题：优先检查相关算子的 `close()`、缓存清理与 tuple 生命周期。

## 当前结论

`big-query` 当前主链不需要新增内核功能；压力生成器已覆盖计划要求并补充少量边界场景。功能代码/测试资产已收口，待同伴编译、调试和压力测试确认。
