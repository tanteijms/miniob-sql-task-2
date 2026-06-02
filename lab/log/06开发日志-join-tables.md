# MiniOB 课程设计开发日志 — join-tables

> 记录时间：2026-06-01  
> 前置：[01](./01开发日志-环境与drop-table.md)～[05](./05开发日志-like.md)

---

## 1. 任务说明

- 支持 `INNER JOIN ... ON` 语法（含链式多表、ON 中 `AND`）
- ON 条件在 join 时过滤；WHERE 仍走原有 `FilterStmt`
- 空表 join 返回空结果
- 官方用例：`test/case/test/primary-join-tables.test`

---

## 2. 测试结果：**通过**

`yys/test/join.sql`（干净库）：

| SQL | 结果 |
|-----|------|
| 两表 `ON id=id` | 2 行 (1,a,1,2)、(2,b,2,15) |
| 三表链式 join | 1 行 |
| `ON ... AND num>13` + `WHERE name='b'` | 1 行 |
| `WHERE name='a'`（num 不满足 ON） | 空 |
| 与空表 join | 空 |

---

## 3. 实现说明

### 3.1 改动文件

| 文件 | 作用 |
|------|------|
| `parse_defs.h` | `FromSqlNode`、`SelectSqlNode::join_conditions` |
| `lex_sql.l` / `yacc_sql.y` | `INNER`/`JOIN`；`from_list` 规则 |
| `select_stmt.cpp/h` | ON 条件 → `FilterStmt` → `ComparisonExpr` |
| `logical_plan_generator.cpp/h` | `create_comparison_expressions`；join 时写入 `JoinLogicalOperator` |
| `nested_loop_join_physical_operator.*` | `next()` 中按 ON 过滤 |
| `physical_plan_generator.cpp` | 将 `join_predicates` 交给 NLJ |
| 删除 `join_physical_operator.*` | 与 NLJ 重复定义导致 ODR/崩溃 |

### 3.2 数据流

```text
FROM t1 INNER JOIN t2 ON t1.id=t2.id AND t2.num>13 WHERE t1.name='b'
  → yacc: relations=[t1,t2], join_conditions=[[id=id, num>13]], conditions=[name=b]
  → SelectStmt: join_predicates[0] = ComparisonExpr 列表
  → LogicalPlan: Join(t1,t2)+predicates → Predicate(WHERE) → Project
  → PhysicalPlan: NestedLoopJoin(ON 过滤) → Predicate → Project
```

### 3.3 注意点

1. **逗号多表**（`FROM t1, t2`）仍走笛卡尔积 + WHERE，与 INNER JOIN 语法并存。
2. 仓库中曾同时编译 `join_physical_operator.cpp` 与 `nested_loop_join_physical_operator.cpp`（同名类），`set_join_predicates` 会段错误；只保留后者。
3. `default_table` 在多表时为 `nullptr`，ON/WHERE 中带表名前缀的列可正常解析。

---

## 4. 自测命令

```bash
bash build.sh debug --make -j8
rm -rf build_debug/bin/miniob/db/sys/*
./yys/scripts/dev.sh run yys/test/join.sql
```

---

## 5. 建议提交（仅 `src/`）

```bash
git add \
  src/observer/sql/parser/parse_defs.h \
  src/observer/sql/parser/lex_sql.l \
  src/observer/sql/parser/yacc_sql.y \
  src/observer/sql/stmt/select_stmt.h \
  src/observer/sql/stmt/select_stmt.cpp \
  src/observer/sql/optimizer/logical_plan_generator.h \
  src/observer/sql/optimizer/logical_plan_generator.cpp \
  src/observer/sql/optimizer/physical_plan_generator.cpp \
  src/observer/sql/operator/nested_loop_join_physical_operator.h \
  src/observer/sql/operator/nested_loop_join_physical_operator.cpp
git add -u src/observer/sql/operator/join_physical_operator.cpp \
           src/observer/sql/operator/join_physical_operator.h
git commit -m "$(cat <<'EOF'
feat: support INNER JOIN with ON predicates in NLJ

EOF
)"
```
