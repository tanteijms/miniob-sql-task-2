# 开发日志：simple-sub-query（4 分）

> 记录时间：2026-06-02  
> 分支：`dev-yys`  
> 前置：complex-sub-query（#7）的必做项

## 目标

- `IN / NOT IN (SELECT …)` 非关联子查询
- 标量比较：`col = (SELECT avg(...))`、子查询在比较符左侧
- 子查询内聚合、空结果集
- 错误：多行标量子查询、`SELECT *` 子查询 → `FAILURE`

## 主要改动

| 模块 | 内容 |
|------|------|
| Parser | `IN`/`NOT`；`select_where` + `where_predicate`；`(SELECT …)` 表达式；`rel_attr comp_op expression` 等 |
| Expr | `UnboundSubQueryExpr` / `SubQueryExpr` / `InSubQueryExpr` |
| Binder | 递归绑定子 `SelectStmt`；`IN` 与标量子查询 |
| Plan | `SelectStmt::where_expression_`；`prepare/open/close_subquery_expressions` |
| 优化 | **禁止**含子查询的 `ComparisonExpr` 被 `PredicatePushdownRewriter` 下推（否则不会 prepare 物理计划） |
| 执行 | `ComparisonExpr::get_value` 对标量侧调用 `materialize_all_values`；`plain_communicator` 执行错误返回 `FAILURE` |

## 自测

```bash
bash build.sh debug --make -j8
python3 lab/test/run_all.py --only custom-simple-sub-query,official-simple-sub-query
```

## 后续

- `EXISTS / NOT EXISTS`（官方 simple 未强制）
- **complex-sub-query**：关联子查询、嵌套子查询

## 踩坑记录

开发过程中 parser / 下推 / prepare / 错误码等 Bug 与误判见：[15踩坑记录-simple-sub-query.md](./15踩坑记录-simple-sub-query.md)
