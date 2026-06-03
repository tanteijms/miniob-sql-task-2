# 开发日志：complex-sub-query（5 分）

> 记录时间：2026-06-02  
> 分支：`dev-yys`  
> 前置：simple-sub-query（4 分）已完成  
> 题目要求：见 `yys/doc/速成/MiniOB.md` §1.21 — 子查询与**父查询联动**；单独执行关联子查询应报错

## 路线

| 步骤 | 内容 | 状态 |
|------|------|------|
| 1 | 嵌套子查询（非关联） | ✅ 基线已支持，无需改代码 |
| 2 | 关联子查询（外层列引用） | ✅ 已实现 |
| 3 | 官方全量 + manifest | ✅ `expected_failure=4`（仅错误用例） |

## 步骤 1：嵌套非关联

16/16 自测 PASS（`custom-complex-sub-query` 前半部分）。

## 步骤 2：关联子查询

### 实现要点（对齐老师提示）

- `BinderContext` 增加 `parent_` 链；子查询绑定时传入父 context
- 外层表字段 → `FieldExpr::outer_ref_`；执行时经 `subquery_outer_tuple()` 读父查询当前行
- `SubQueryExpr::correlated_`：含外层引用或嵌套关联子查询时标记；**不缓存**物化结果
- 嵌套关联：外层 tuple 栈仅在栈空时 push，避免内层 scan tuple 覆盖主查询行

### 改动文件（`src/`）

| 模块 | 内容 |
|------|------|
| `expression_binder.h/.cpp` | 父 scope；外层字段 `outer_ref`；子查询 `SelectStmt::create(..., &context_)` |
| `select_stmt.h/.cpp` | `create` 可选 `parent_context` |
| `expression.h/.cpp` | `FieldExpr::outer_ref_`；标量比较传递 outer tuple |
| `subquery_expr.h/.cpp` | correlated 标记、outer context 栈、IN/标量按行重算 |

### 自测

```bash
bash build.sh debug --make -j8
python3 lab/test/run_all.py --only custom-complex-sub-query,official-complex-sub-query
python3 lab/test/run_all.py --quick
```

- 官方 `primary-complex-sub-query.test`：**FAILURE 4**（L51–54 多行标量 / `SELECT *` 错误用例）
- 关联用例 L38、L40 已通过

## 相关文件

- 自测 SQL：`lab/test/cases/sql/complex-sub-query.sql`（18 条 SELECT）
- manifest：`official-complex-sub-query` / `custom-complex-sub-query`
- 前置踩坑：`lab/log/15踩坑记录-simple-sub-query.md`
