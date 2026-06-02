# MiniOB 课程设计开发日志 — like

> 记录时间：2026-06-02  
> 前置：[01](./01开发日志-环境与drop-table.md)～[04](./04开发日志-aggregation-func.md)

---

## 1. 任务说明

- 在 `WHERE` 中支持 `LIKE`：`列 LIKE '模式'`  
- `%`：匹配 0 个或多个任意字符（**不含**英文单引号 `'`）  
- `_`：匹配恰好 1 个任意字符（**不含** `'`）  
- 仅需支持 **CHAR** 字段  

---

## 2. 测试结果：**通过**

`yys/test/like.sql`（干净库）：

| SQL | 结果 |
|-----|------|
| `name LIKE 'abc'` | id=1 `abc` |
| `name LIKE 'abc%'` | id=1,2 |
| `name LIKE '%bc_'` | id=2 `abcd` |
| `name LIKE '_bc'` | id=1 `abc` |
| `name LIKE 'a_c%'` | id=1,2 |

---

## 3. 实现说明

### 3.1 改动文件

| 文件 | 作用 |
|------|------|
| `parse_defs.h` | `CompOp` 增加 `LIKE_OP` |
| `lex_sql.l` / `yacc_sql.y` | `LIKE` 关键字与 `comp_op` |
| `like_match.h/cpp` | 模式匹配（递归处理 `%` / `_`） |
| `expression.cpp` | `ComparisonExpr::compare_value` 增加 `LIKE_OP` |
| `logical_plan_generator.cpp` | LIKE 不做数值隐式转换 |
| `condition_filter.cpp` | 旧路径兼容 LIKE |

### 3.2 数据流

```text
WHERE name LIKE 'abc%'
  → yacc: ConditionSqlNode(comp=LIKE_OP)
  → FilterStmt → ComparisonExpr
  → PredicatePhysicalOperator
  → compare_value → like_match(列值, 模式串)
```

---

## 4. 怎么测

```bash
export PATH="/opt/homebrew/opt/bison/bin:$PATH"
cd /Users/yishuoyan/projects/bupt/25-26-2/miniob-sql-task-2
bash build.sh debug --make -j8
rm -f build_debug/bin/miniob/db/sys/lk*
./yys/scripts/dev.sh run test/like.sql
```

---

## 5. 建议 commit

```bash
git add src/observer/sql/parser/parse_defs.h \
        src/observer/sql/parser/lex_sql.l \
        src/observer/sql/parser/yacc_sql.y \
        src/observer/sql/expr/like_match.h \
        src/observer/sql/expr/like_match.cpp \
        src/observer/sql/expr/expression.cpp \
        src/observer/sql/optimizer/logical_plan_generator.cpp \
        src/observer/storage/common/condition_filter.cpp

git commit -m "$(cat <<'EOF'
feat: implement SQL LIKE for CHAR columns in WHERE

Add LIKE parser token, LIKE_OP comparison, and pattern matching
with % and _ wildcards (excluding single quote).
EOF
)"
```

---

## 6. 进度

| 题目 | 状态 |
|------|------|
| drop-table / update / date / aggregation | ✅ |
| **like** | ✅ |
| join-tables 等 | ⏳ 暂缓 |

简单题累计约 **13 分**（含 basic）。
