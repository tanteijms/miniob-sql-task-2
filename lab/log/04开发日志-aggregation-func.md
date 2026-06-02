# MiniOB 课程设计开发日志 — aggregation-func

> 仓库：`miniob-sql-task-2`  
> 分支：`dev-yys`（或 `bupt-lab`）  
> 个人工作区：`yys/`（不提交）  
> 记录时间：2026-06-02  
> 前置：[01](./01开发日志-环境与drop-table.md) / [02](./02开发日志-update.md) / [03](./03开发日志-date.md)

---

## 1. 任务说明

### 1.1 功能要求

- 实现聚合函数：**count / min / max / avg / sum**（本题以 count/min/max/avg 为主）  
- 典型 SQL：`SELECT count(*) FROM t;`、`SELECT min(num), max(num), avg(num) FROM t;`  
- **错误语义**（应 FAILURE）：`min(*)`、`count(*,col)`、空参数 `count()`、不存在的列等  
- 混合「普通列 + 聚合」且无 `GROUP BY` → FAILURE（框架已有检查）

### 1.2 改前状态

| 层次 | 状态 |
|------|------|
| 语法 `aggregate_expression` | ✅ 已有 |
| `ExpressionBinder::bind_aggregate_expression` | ✅ 已有（含 `count(*)` → 常量 1） |
| `LogicalPlanGenerator::create_group_by_plan` | ✅ 无 GROUP BY 时插 `GroupByLogicalOperator` |
| `ScalarGroupByPhysicalOperator` | ✅ 火山模型扫表聚合 |
| `AggregateExpr::create_aggregator` | ❌ **仅实现 SUM**，其余 assert 崩溃 |
| `aggregator.cpp` | ❌ 只有 `SumAggregator` |

---

## 2. 测试结果

### 2.1 结论：**通过**

`yys/test/aggregation.sql`（干净库）：

| SQL | 期望 | 实测 |
|-----|------|------|
| `count(*)` | 4 | ✅ |
| `count(num)` | 4 | ✅ |
| `min(num)` / `max(num)` / `avg(num)` | 12 / 18 / 15 | ✅ |
| `min(price)` | 10.0 | ✅ |
| `max(addr)` | `dei`（字符序最大） | ✅ |
| 多聚合同 SELECT | 一行三列 | ✅ |
| `min(*)` | FAILURE | ✅ |
| `count(id2)` | FAILURE | ✅ |

官方用例：`test/case/test/primary-aggregation-func.test`（含 date 列，需 date 题已完成）。

---

## 3. 实现说明

### 3.1 调用链

```text
SELECT count(*), min(num) FROM t
  → Parser: UnboundAggregateExpr
  → ExpressionBinder: AggregateExpr + check_aggregate_expression
  → create_group_by_plan: GroupByLogicalOperator（无 GROUP BY 列）
  → ScalarGroupByPhysicalOperator
       → 子算子 TableScan 逐行
       → Aggregator::accumulate
       → evaluate → 一行结果
  → Project 输出
```

### 3.2 新增聚合器（`aggregator.h` / `aggregator.cpp`）

| 类 | accumulate | evaluate |
|----|------------|----------|
| `CountAggregator` | 每行 +1 | `set_int(count_)` |
| `MinAggregator` | `Value::compare` 取小 | 首行或最小值 |
| `MaxAggregator` | 取大 | 同上 |
| `AvgAggregator` | `sum += get_float()` | `sum/count` 为 float |
| `SumAggregator` | 原有 | 保留 |

`AggregateExpr::create_aggregator()` 按类型返回对应实例。

### 3.3 其它修改

| 文件 | 改动 |
|------|------|
| `expression_binder.cpp` | 仅 `COUNT` 允许 `*`；`min(*)` 等直接 `INVALID_ARGUMENT` |
| `scalar_group_by_physical_operator.cpp` | **空表**也生成聚合器并 `evaluate`（`count(*)=0`） |

### 3.4 未改

- 语法、SelectStmt、向量化 `AggregateVecPhysicalOperator`（非默认路径）  
- `sum()` 聚合器已有，本题测试未重点覆盖  

---

## 4. 怎么测

```bash
export PATH="/opt/homebrew/opt/bison/bin:$PATH"
cd /Users/yishuoyan/projects/bupt/25-26-2/miniob-sql-task-2
bash build.sh debug --make -j8
./yys/scripts/dev.sh run test/aggregation.sql
```

---

## 5. 建议 commit

```bash
git add src/observer/sql/expr/aggregator.h \
        src/observer/sql/expr/aggregator.cpp \
        src/observer/sql/expr/expression.cpp \
        src/observer/sql/parser/expression_binder.cpp \
        src/observer/sql/operator/scalar_group_by_physical_operator.cpp

git commit -m "$(cat <<'EOF'
feat: implement COUNT/MIN/MAX/AVG aggregators for scalar GROUP BY

Add aggregator implementations and wire create_aggregator; reject
STAR for non-COUNT aggregates; handle empty-table aggregation.
EOF
)"
```

---

## 6. 进度

| 题目 | 状态 |
|------|------|
| basic / drop-table / update / date | ✅ |
| **aggregation-func** | ✅ |
| like | ⏳ 建议下一题（3 分） |

---

## 7. 修订记录

| 日期 | 内容 |
|------|------|
| 2026-06-02 | 初稿：聚合器实现与自测 |
