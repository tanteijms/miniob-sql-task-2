# 踩坑记录：simple-sub-query 开发中的 Bug 与误判

> 记录时间：2026-06-02  
> 分支：`dev-yys`  
> 关联：[14开发日志-simple-sub-query.md](./14开发日志-simple-sub-query.md)

本文专门记录实现 **simple-sub-query** 时走弯路、修错、或一度误判根因的问题，方便以后做 **complex-sub-query** 时对照。

---

## 1. 总览

| # | 现象（一句话） | 真因 | 严重度 |
|---|----------------|------|--------|
| 1 | `col1 = (SELECT …)` 解析失败 | WHERE 只有 `rel_attr comp_op value`，括号右值无规则 | 高 |
| 2 | 改了 yacc 不生效 | `yacc_sql.cpp` 未重新生成 / 构建用到旧产物 | 中 |
| 3 | `IN (… WHERE 1=0)` ASAN 崩溃 | 子查询逻辑计划重复生成 + `filter_stmt==nullptr` 仍走旧路径 | 高 |
| 4 | **IN 正常，标量比较永远空结果** | 谓词下推把含子查询的 `ComparisonExpr` 推走，未 `prepare` 物理计划 | **致命** |
| 5 | 多行标量 / 除零仍显示 SUCCESS | `plain_communicator` 执行中途出错未写回 `FAILURE` | 中 |
| 6 | 误以为 `SubQueryExpr::open` 时机问题 | 表象像物化失败，实际是 #4 | 低（误判） |

---

## 2. Bug #1：标量比较的 WHERE 解析失败

### 现象

```sql
SELECT col1 FROM ssq_1 WHERE col1 = (SELECT avg(col2) FROM ssq_2);
-- SQL_SYNTAX 或无法按预期解析
```

而下面可以：

```sql
SELECT * FROM ssq_1 WHERE id IN (SELECT id FROM ssq_2);  -- OK
```

### 根因

`WHERE` 里原先大量规则是 **`rel_attr comp_op value`**（如 `col1 = 1`）。  
Bison 在看到 `col1 =` 后会优先走这条产生式；右侧遇到 `(` 时期望 **value**，不是 `(SELECT …)`，导致解析失败。

`IN (SELECT …)` 有独立规则 `expression IN LBRACE select_subquery RBRACE`，不受影响。

### 修复

在 `where_predicate` **最前面**增加（并补全子查询在左侧的情况）：

- `rel_attr comp_op expression`
- `expression comp_op rel_attr`
- `LBRACE select_subquery RBRACE comp_op expression`
- `LBRACE select_subquery RBRACE comp_op rel_attr`

### 教训

- **不要假设** `expression comp_op expression` 能覆盖 `col1 = …`：shift/reduce 会优先匹配更具体的 `rel_attr comp_op value`。
- 标量比较和 `IN` 要在语法层分开考虑，不能只看 `IN` 能跑就认为 `=` 也能跑。

---

## 3. Bug #2：修改 yacc 后行为不变

### 现象

已改 `yacc_sql.y`（加 `IN`、`select_where` 等），但 observer 仍像旧 parser，`UnboundSubQuery` 相关 include 缺失等。

### 根因

CMake 虽配置了 `bison_target`，本地有时 **未触发 regen**，或曾手动改过 `yacc_sql.cpp` 与 `.y` 不同步。

### 修复

```bash
cd src/observer/sql/parser
bison -d yacc_sql.y -o yacc_sql.cpp
bash build.sh debug --make -j8
```

### 教训

Parser 行为异常时，**先确认 `yacc_sql.cpp` 里是否出现新 token/规则**，再查执行层。

---

## 4. Bug #3：子查询带 `WHERE 1=0` 时 ASAN SEGV

### 现象

```sql
SELECT * FROM ssq_1 WHERE id IN (SELECT id FROM ssq_2 WHERE 1=0);
-- exit 134, AddressSanitizer: FilterUnit vector begin
```

栈：`LogicalPlanGenerator::create_comparison_expressions(FilterStmt*)` → `filter_stmt->filter_units()` 空指针。

### 根因（两处叠加）

1. **`prepare_subquery_expressions` 对同一棵表达式树多次遍历**，可能对同一 `SubQueryExpr` 重复调用 `LogicalPlanGenerator::create(select_stmt)`。  
   第一次会把 `where_expression` **move 走**；第二次 `where_expression()` 为空，落入 `create_plan(filter_stmt)`，而 `filter_stmt` 也是 **nullptr** → 解引用崩溃。

2. `create_plan(SelectStmt*)` 里原先写法：

   ```cpp
   if (where_expression()) { ... }
   else { create_plan(filter_stmt(), ...); }  // filter_stmt 可能为 null
   ```

### 修复

- `prepare_subquery_expr`：若 `physical_operator_ != nullptr` 则 **跳过** 重复 prepare。
- 逻辑计划：`else if (select_stmt->filter_stmt() != nullptr)` 再调 `create_plan(filter_stmt)`。

### 教训

- 子查询的 `SelectStmt` 是 **可消耗对象**（`where_expression`、`query_expressions` 会被 move 到逻辑计划）。
- `prepare` 必须 **幂等**，或只执行一次。

---

## 5. Bug #4（核心）：IN 能用，标量比较永远 0 行

### 现象

| SQL | 结果 |
|-----|------|
| `WHERE col1 IN (SELECT col2 FROM ssq_2)` | 正常 |
| `WHERE col1 = (SELECT col2 FROM ssq_2 WHERE col2=4)` | 只有表头，0 行 |
| `WHERE col1 = (SELECT avg(col2) FROM ssq_2)` | 同上 |

调试发现：

- `ComparisonExpr::get_value` 里 **right 已是 `SUBQUERY`（type=14）**；
- 但 `SubQueryExpr::materialize_all_values` 返回 **`physical_operator_ == nullptr` → INTERNAL**；
- 对 **IN** 查询：`prepare_subquery_expressions` 有日志；对 **标量 =** 查询：**完全没有 prepare 日志**，也 **没有** `physical predicate plan` 日志。

### 根因

优化阶段 **`PredicatePushdownRewriter`** 的行为：

1. 对所有 `ComparisonExpr`（含 `col1 = (SELECT …)`）执行 **谓词下推** → 表达式挂到 `TableGetLogicalOperator::predicates_`。
2. 上层 `PredicateLogicalOperator` 被换成恒真 `ValueExpr(true)`。
3. **`PredicateRewriteRule`** 再把这个恒真谓词删掉，计划变成 **`Project → TableScan`**，中间 **没有 `PredicatePhysicalOperator`**。
4. `prepare_subquery_expressions` 只在 **`PhysicalPlanGenerator::create_plan(PredicateLogicalOperator&)`** 里调用 → **标量子查询从未 prepare**，`physical_operator_` 一直为空。

**IN 子查询** 的根表达式类型是 `IN_SUBQUERY`，不是 `COMPARISON`，**不会被下推**，所以仍保留 `Predicate` 节点 → prepare 正常。

这是本次 **最隐蔽、耗时最长** 的 bug：表象像「物化/比较逻辑错了」，实际是 **计划被优化器改没了**。

### 修复

`predicate_pushdown_rewriter.cpp`：

- 若表达式（含子树）中出现 `SUBQUERY` / `IN_SUBQUERY` / `UNBOUND_SUBQUERY`，**禁止下推**；
- 对 `IN_SUBQUERY` 整棵也不下推。

另在 `ComparisonExpr::get_value` 中，对 `SUBQUERY` 操作数显式走 `materialize_all_values`（双保险）。

### 教训

- 新增表达式类型时，必须检查 **所有 RewriteRule**（尤其 `PredicatePushdownRewriter`、`ExpressionRewriter`）。
- 排查子查询执行问题时的推荐顺序：
  1. 逻辑计划里是否还有 **PREDICATE** 节点？
  2. 物理计划是否调用 **prepare_subquery_expressions**？
  3. 再查 **materialize / get_value**。

---

## 6. Bug #5：错误语义返回 SUCCESS 而非 FAILURE

### 现象

官方 error 段：

```sql
SELECT * FROM ssq_1 WHERE col1 = (SELECT col2 FROM ssq_2);  -- 多行标量，应 FAILURE
```

实际：打印表头后 **仍显示 SUCCESS**。

### 根因

`PredicatePhysicalOperator::next()` 在 `get_value` 返回 `INVALID_ARGUMENT` 时会 **带错返回**；  
但 `PlainCommunicator::write_tuple_result` 收到非 SUCCESS 的 rc 后 **没有** `sql_result->set_return_code(rc)`，最终 `write_state` 仍输出 SUCCESS。

### 修复

`plain_communicator.cpp`：若 `write_tuple_result` / `write_chunk_result` 失败，设置 return_code 并 `write_state`。

### 连带影响

`primary-expression.test` 里 **除零** 等中途失败语句，现在也会正确记为 FAILURE，manifest 中 `official-expression` 的 `expected_failure` 从 **1 调整为 2**（行为更正确，不是回退）。

---

## 7. 误判与无效尝试（记录以免重蹈覆辙）

### 7.1 「标量物化 cache / open 时机不对」

曾怀疑：

- 在 `PredicatePhysicalOperator::open` 里过早 `SubQueryExpr::open`，与 `materialize` 里 close/reopen 冲突；
- `scalar_materialized_` 缓存了空结果。

改动：改为 `open_subquery` 只 `set_trx`，物化时再 open 算子。

**结果**：标量比较仍空。根因仍是 **#4 未 prepare**，与 open 时机无关。

### 7.2 「value_expr_ 拷贝 vs Project 表达式不一致」

曾改为 `tuple->cell_at(0)` 取子查询列，避免 `value_expr_` 副本与算子内表达式不一致。

**结果**：对 #4 无效（physical 算子根本不存在）；在 #4 修好后，cell_at 仍可作为稳健取值的实现细节保留。

### 7.3 「int 与 float 比较」

曾怀疑 `avg` 返回 float、`col1` 为 int 导致比较恒 false。

**结果**：`IntegerType::compare` 已支持对 float 比较；单独 `WHERE col1=4` 正常，排除此因。

---

## 8. 调试手法备忘

本次有效的定位步骤：

1. **对比 IN vs =**：同一子查询，一种能跑一种不能 → 优先查计划结构差异，而非子查询实现本身。
2. **stderr 打点**（临时）：
   - `logical where type=` / `physical predicate plan` / `prepare_subquery_expressions type=`
   - `ComparisonExpr::get_value right_type=` / `physical_operator is null`
3. **官方用例分层跑**：`lab/test/run_all.py --only custom-simple-sub-query,official-simple-sub-query`
4. **ASAN 栈**：`FilterUnit` 崩溃 → 查 `filter_stmt` 空指针与重复 `create_plan`。

---

## 9. 涉及文件索引（Fix 汇总）

| 文件 | 与哪些 Bug 相关 |
|------|-----------------|
| `yacc_sql.y` | #1 |
| `logical_plan_generator.cpp` | #3 |
| `subquery_expr.cpp` / `subquery_expr.h` | #3 #4 #6 |
| `predicate_pushdown_rewriter.cpp` | **#4** |
| `expression.cpp` | #4 |
| `plain_communicator.cpp` | #5 |
| `lab/test/manifest.json` | #5（expression expected_failure） |

---

## 10. 对 complex-sub-query 的提醒

做关联 / 嵌套子查询前，建议先确认：

1. **BinderContext 作用域链** — 外层列能否在子查询 WHERE 解析；
2. **下推规则** — 任何含子查询的表达式默认 **不可下推**；
3. **prepare 幂等** — 嵌套子查询树深度增加，重复 prepare 更容易踩 #3；
4. **关联子查询** — 不能只做一次 materialize，需按外层行重算或相关执行（与本次非关联缓存不同）。
