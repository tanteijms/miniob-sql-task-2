# MiniOB 课程设计开发日志 — order-by

> 记录时间：2026-06-01  
> 前置：[07](./07开发日志-function.md)

---

## 1. 任务说明

- `ORDER BY col [ASC|DESC]`，默认 ASC
- 多列排序、与 `WHERE` 组合、多表 `table.col` 排序
- 官方用例：`test/case/test/primary-order-by.test`

---

## 2. 测试结果：**通过**

`yys/test/order-by.sql` 覆盖单表 ASC/DESC、多列、WHERE+ORDER、多表 JOIN+ORDER。

---

## 3. 实现说明

### 3.1 计划结构

```text
TableGet / Join → Predicate → GroupBy(若有) → Sort → Project
```

### 3.2 改动文件

| 文件 | 作用 |
|------|------|
| `parse_defs.h` | `OrderBySqlNode`、`SelectSqlNode::order_by` |
| `lex_sql.l` / `yacc_sql.y` | `ORDER BY`、ASC/DESC |
| `select_stmt.cpp/h` | 绑定排序表达式 |
| `sort_logical_operator.h` | 逻辑排序算子 |
| `sort_physical_operator.*` | 物化子算子输出后 `std::sort` |
| `logical/physical_plan_generator.*` | 接入 SORT 节点 |

### 3.3 注意

- `DESC` 与 `DESC TABLE` 共用词法 token，无冲突。
- 排序在 **Project 之前**，可对未投影列排序。

---

## 4. 建议提交

```bash
git add \
  src/observer/sql/parser/parse_defs.h \
  src/observer/sql/parser/lex_sql.l \
  src/observer/sql/parser/yacc_sql.y \
  src/observer/sql/stmt/select_stmt.h \
  src/observer/sql/stmt/select_stmt.cpp \
  src/observer/sql/operator/logical_operator.h \
  src/observer/sql/operator/physical_operator.h \
  src/observer/sql/operator/physical_operator.cpp \
  src/observer/sql/operator/sort_logical_operator.h \
  src/observer/sql/operator/sort_physical_operator.h \
  src/observer/sql/operator/sort_physical_operator.cpp \
  src/observer/sql/optimizer/logical_plan_generator.cpp \
  src/observer/sql/optimizer/physical_plan_generator.h \
  src/observer/sql/optimizer/physical_plan_generator.cpp

git commit -m "$(cat <<'EOF'
feat: support ORDER BY with multi-column ASC/DESC sort

EOF
)"
```

---

## 5. 后续建议

| 题目 | 分值 |
|------|------|
| null | 3 |
| simple-sub-query | 4 |
| expression（若未做） | 视分支而定 |
