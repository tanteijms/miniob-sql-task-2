# MiniOB 课程设计开发日志 — group-by

> 仓库：`miniob-sql-task-2`  
> 分支：`dev-yys`  
> 个人工作区：`yys/`（不提交）  
> 记录时间：2026-06-02  
> 前置：[08](./08开发日志-order-by.md)

---

## 1. 任务说明

### 1.1 功能边界

| 能力 | 说明 |
|------|------|
| 多字段 `GROUP BY` | 单/多列分组键 |
| 聚合函数 | count / min / max / avg（依赖 [04](./04开发日志-aggregation-func.md)） |
| `HAVING` | 聚合**之后**过滤（本次主要增量） |
| 组合 | `WHERE` + `GROUP BY` + `HAVING` + `ORDER BY`、多表 JOIN 后分组 |

### 1.2 官方用例

- `test/case/test/primary-group-by.test`  
- 注：该文件中 SELECT 多为 `-- sort select ...` 注释行，本地需用 `yys/test/group-by.sql` 或取消注释验证

---

## 2. 测试结果：**通过**

### 2.1 自测命令

```bash
export PATH="/opt/homebrew/opt/bison/bin:$PATH"
bash build.sh debug --make -j8
rm -rf build_debug/bin/miniob/db/sys/*
./yys/scripts/dev.sh run yys/test/group-by.sql
```

### 2.2 典型 SQL 与期望

| SQL | 期望要点 |
|-----|----------|
| `select id, avg(score) from t group by id` | id=3→2.4，id=1→2，id=4→3 |
| `select id, name, avg(score) from t group by id, name` | 按 (id,name) 多行分组 |
| `select id, avg(score) from t where id>2 group by id` | 仅 id=3、4 |
| 多表 `where t1.id=t2.id group by t1.id, t1.name` | 与官方样例一致 |
| `... group by id having avg(score) > 2` | 仅 id=3(2.4)、id=4(3) |
| `... group by name having count(id) > 1` | 仅 name='c'(3) |
| `... group by id having id > 2` | 分组键直接过滤 |
| `select count(*) from t having count(*) > 5` | 一行 7 |
| `... having avg(score)>2 order by id desc` | 4→3 两行 |

### 2.3 EXPLAIN 计划

```text
select id, avg(score) from t_group_by group by id having id > 2;

Query Plan
OPERATOR(NAME)
PROJECT
└─PREDICATE          ← HAVING
  └─HASH_GROUP_BY
    └─TABLE_SCAN(t_group_by)
```

---

## 3. 改前状态

| 层次 | 状态 |
|------|------|
| 语法 `GROUP BY expression_list` | ✅ 已有 |
| `HashGroupByPhysicalOperator` / `ScalarGroupByPhysicalOperator` | ✅ 已有 |
| `LogicalPlanGenerator::create_group_by_plan` | ✅ 已有（聚合收集、非聚合列校验） |
| **`HAVING` 语法与计划** | ❌ 缺失 |

基础 GROUP BY 在改前已能跑通官方 SELECT 语义；本次补 HAVING 全链路。

---

## 4. 实现说明

### 4.1 执行计划顺序

```text
TableGet / Join
  → Predicate (WHERE)
  → GroupBy (Hash / Scalar)
  → Predicate (HAVING)    ← 新增，必须在聚合之后
  → Sort (ORDER BY)
  → Project
```

**规避点**：不可把 HAVING 当 WHERE 提前执行，否则语义错误。

### 4.2 改动文件

| 文件 | 作用 |
|------|------|
| `lex_sql.l` | 增加 `HAVING` 关键字 |
| `yacc_sql.y` | `SELECT ... group_by having order_by`；`having_condition`: `expr comp_op value/expr` |
| `parse_defs.h` | `SelectSqlNode::having` |
| `select_stmt.h/cpp` | 绑定 HAVING 为 `ComparisonExpr` / `ConjunctionExpr(AND)` |
| `logical_plan_generator.cpp` | ① HAVING 表达式参与 `create_group_by_plan` 聚合收集与列校验 ② GroupBy 后插入 `PredicateLogicalOperator` |

### 4.3 已有能力（未改动物理算子）

- `HashGroupByPhysicalOperator`：多列哈希分组 + 聚合求值  
- `ScalarGroupByPhysicalOperator`：无 GROUP BY 仅聚合；空表输出一行  
- `create_group_by_plan`：`found_unbound_column` 校验 SELECT 列表

### 4.4 Bug 修复：HAVING 绑定后表达式为空

`ExpressionBinder::bind_expression` 对 `ComparisonExpr` 会 `std::move(expr)` 到 `bound` 向量。  
若绑定后仍从 `select_sql.having[0]` 取表达式，得到的是 **空 unique_ptr**，导致：

- 逻辑计划中无 `PREDICATE` 节点  
- HAVING 条件永不生效，结果与无 HAVING 相同  

**修复**：绑定后从 `bound[0]` 移入 `bound_having`，再组装 ConjunctionExpr。

---

## 5. 建议提交

```bash
git add \
  src/observer/sql/parser/parse_defs.h \
  src/observer/sql/parser/lex_sql.l \
  src/observer/sql/parser/yacc_sql.y \
  src/observer/sql/stmt/select_stmt.h \
  src/observer/sql/stmt/select_stmt.cpp \
  src/observer/sql/optimizer/logical_plan_generator.cpp

git commit -m "$(cat <<'EOF'
feat: support GROUP BY HAVING filter after aggregation

Parse HAVING comparisons, bind expressions, and insert a predicate
operator after hash/scalar group-by; collect HAVING aggregates in plan.
EOF
)"
```

---

## 6. 进度索引（lab/log）

| 序号 | 题目 | 日志 |
|------|------|------|
| 01 | 环境 + drop-table | [01](./01开发日志-环境与drop-table.md) |
| 02 | update | [02](./02开发日志-update.md) |
| 03 | date | [03](./03开发日志-date.md) |
| 04 | aggregation-func | [04](./04开发日志-aggregation-func.md) |
| 05 | like | [05](./05开发日志-like.md) |
| 06 | join-tables | [06](./06开发日志-join-tables.md) |
| 07 | function | [07](./07开发日志-function.md) |
| 08 | order-by | [08](./08开发日志-order-by.md) |
| **09** | **group-by** | **本文** |

---

## 7. 后续建议（见 `lab/todo/top8-最值得做的任务.md`）

| 题目 | 分值 | 说明 |
|------|------|------|
| null | 3 | 与聚合/比较语义耦合 |
| simple-sub-query | 4 | IN / 标量子查询 |
| multi-index | 4 | 联合索引 |
