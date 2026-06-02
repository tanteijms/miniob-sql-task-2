# MiniOB 课程设计开发日志 — function

> 记录时间：2026-06-01  
> 前置：[06](./06开发日志-join-tables.md)

---

## 1. 任务说明

实现标量函数（仅考虑指定类型，否则整条语句 **FAILURE**）：

| 函数 | 参数类型 | 返回 |
|------|----------|------|
| `LENGTH` | `CHAR` | `INT`（去掉尾部空格后的长度） |
| `ROUND` | `FLOAT` | `FLOAT`（四舍五入到整数） |
| `DATE_FORMAT` | `DATE`, `CHAR` 格式串 | `CHAR` |

---

## 2. 测试结果：**通过**

`yys/test/function.sql`：

| SQL | 结果 |
|-----|------|
| `LENGTH(name)` | 3, 4 |
| `ROUND(score)` | 2, 3 |
| `DATE_FORMAT(u_date,'%Y-%m-%d')` | 2020-01-21, 2016-02-29 |
| `DATE_FORMAT(...,'%D,%M,%Y')` | 21st,January,2020 |
| `LENGTH(score)` / `ROUND(name)` | FAILURE |

---

## 3. 实现说明

### 3.1 改动文件

| 文件 | 作用 |
|------|------|
| `sql_function.h/cpp` | 三个函数求值 |
| `expression.h/cpp` | `UnboundFunctionExpr` / `FunctionExpr` |
| `expression_binder.cpp` | 绑定与类型校验 |
| `expression_iterator.cpp` | 遍历子表达式 |
| `lex_sql.l` / `yacc_sql.y` | `LENGTH`/`ROUND`/`DATE_FORMAT` 语法 |

### 3.2 注意

- `DATE_FORMAT` 词法规则须写在 `DATE` 之前，避免被拆成 `DATE` + `_FORMAT`。
- 错误类型在 `ExpressionBinder` 阶段返回 `INVALID_ARGUMENT`，语句表现为 FAILURE。

---

## 4. 建议提交

```bash
git add \
  src/observer/sql/expr/sql_function.h \
  src/observer/sql/expr/sql_function.cpp \
  src/observer/sql/expr/expression.h \
  src/observer/sql/expr/expression.cpp \
  src/observer/sql/expr/expression_iterator.cpp \
  src/observer/sql/parser/expression_binder.h \
  src/observer/sql/parser/expression_binder.cpp \
  src/observer/sql/parser/lex_sql.l \
  src/observer/sql/parser/yacc_sql.y

git commit -m "$(cat <<'EOF'
feat: add LENGTH, ROUND, and DATE_FORMAT scalar functions

EOF
)"
```

---

## 5. 后续题目建议

| 顺序 | 题目 | 分值 |
|------|------|------|
| 1 | order-by | 4 |
| 2 | null | 3 |
| 3 | simple-sub-query | 4 |
