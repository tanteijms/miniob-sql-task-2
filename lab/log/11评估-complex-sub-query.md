# 评估：complex-sub-query（5 分，困难题）

> 记录时间：2026-06-02  
> 前置建议：[simple-sub-query 未做]

---

## 1. 结论：**现在不好直接做**

| 维度 | 现状 |
|------|------|
| 代码库 | **零子查询实现**（无 `SubQueryExpr`、parser 不支持 `(SELECT …)`） |
| 前置题 | **simple-sub-query（4 分）未做**，complex 全部建立在其上 |
| 工作量 | 社区经验：「相当于重构一遍查询路径」（parser → binder → expr → 执行） |
| 预估改动 | 15～25 个文件，2000+ 行，多轮自测 |

**建议顺序**：`simple-sub-query` → 再 `complex-sub-query`（嵌套 + 关联）。

---

## 2. 官方用例要什么

### simple-sub-query（必须先过）

- `IN / NOT IN (SELECT …)`
- 标量比较 `(SELECT avg(...)) = col`
- 子查询内聚合
- **非关联**（子查询不引用外层列）
- 错误：`col = (SELECT col2 FROM t2)` 多行 → FAILURE

### complex-sub-query（在 simple 之上）

| 能力 | 示例 |
|------|------|
| 嵌套子查询 | `id IN (SELECT … WHERE id IN (SELECT …))` |
| 子查询内聚合 + 嵌套 | `col1 > (SELECT avg … WHERE feat >= (SELECT min …))` |
| **关联子查询** | `feat <> (SELECT avg … WHERE inner.f > **outer.f**)` |
| 关联 + IN 嵌套 | `… WHERE csq_2.id IN (SELECT … WHERE **csq_1.id** = csq_3.id)` |

官方测试：`primary-complex-sub-query.test`（SELECT 多为 `-- sort` 注释，需本地跑 SQL）。

---

## 3. 与本仓库现状的差距

当前 WHERE 路径：

```text
yacc condition_list (rel_attr comp_op value)
  → FilterStmt / FilterUnit
  → LogicalPlanGenerator::create_comparison_expressions
  → PredicateLogicalOperator(ComparisonExpr)
```

子查询需要：

```text
expression [NOT] IN (SELECT …)  /  expr comp_op (SELECT …)
  → SubQueryExpr + 绑定 SelectStmt
  → get_value 时执行子查询物理算子（或预物化结果集）
  → 关联子查询：BinderContext **scope 栈** + 外层 Tuple 传入
```

已有可复用：

- `SelectStmt` / `LogicalPlanGenerator` / 聚合 / GROUP BY / JOIN
- `Expression` 框架（`ComparisonExpr`、`ConjunctionExpr`）
- `PredicatePhysicalOperator` 按 tuple 求值

缺失：

- Parser：`(SELECT …)` 作为表达式/条件
- `ExprType::SUBQUERY`（或 `IN_SUBQUERY`）
- `ExpressionBinder` 递归绑定子 `SelectStmt`
- 执行：非关联缓存 vs 关联逐行重算
- WHERE 从 `FilterStmt` 迁到表达式（或双路径并存）

---

## 4. 推荐实施路线（若要做）

### 阶段 A — simple-sub-query（4 分，1～2 天）

1. yacc：`expression IN (select_stmt)`、`NOT IN`、标量 `(SELECT 单列 …)`
2. `SubQueryExpr`：持 `SelectSqlNode` → 绑定后持 `unique_ptr<PhysicalOperator>` 或 `SelectStmt`
3. `ComparisonExpr` 扩展或新增 `InExpr`：`left IN subquery`
4. 非关联执行：子查询 **执行一次**，结果放 `vector<Value>` / hash set
5. 标量：子查询必须 0/1 行，否则 FAILURE
6. 自测：`primary-simple-sub-query.test`

### 阶段 B — complex 增量（+1 分困难题，1～2 天）

1. 子查询内 WHERE 再含子查询（递归绑定同一套 `SubQueryExpr`）
2. `BinderContext` 父 scope 链：解析 `csq_1.id` 时先查子查询表，再查外层表
3. `SubQueryExpr::get_value(tuple)`：关联时把 **外层 tuple** 传给子查询算子 / 相关谓词
4. 自测：`primary-complex-sub-query.test` 中带 `csq_1.xxx` 的用例

### 风险

- 作用域混乱 → 显式 `BinderContext` 栈，单元测 `outer.id = inner.id`
- NOT IN + NULL → 按题目约定（MiniOB 多数测例无 NULL 子查询）
- 性能：非关联务必缓存，关联只能 NLJ 式重算

---

## 5. 与 top8 其他题对比

| 题目 | 分值 | 前置 | 建议 |
|------|------|------|------|
| null | 3 | 无 | 独立，可并行 |
| **simple-sub-query** | 4 | 无 | **应先于 complex** |
| **complex-sub-query** | 5 | simple | 答辩加分，但工期最长 |
| update-select | 4 | 子查询 | 在 simple 之后 |

---

## 6. 当前决策

**本次不启动 complex-sub-query 编码**——前置 simple 为零，直接做会半途卡住且无法过测。

**下一步建议**：开 `simple-sub-query`，通过后再开 complex。若你确认，可从 parser + `SubQueryExpr` 骨架开始（`lab/log/12开发日志-simple-sub-query.md`）。
