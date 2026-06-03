# 开发日志：update-select（4 分）

> 记录时间：2026-06-02  
> 分支：`dev-yys`  
> 前置：simple-sub-query、complex-sub-query、null  
> 题目要求：见 `lab/todo/top8-最值得做的任务.md` §5 — `UPDATE t SET c = (SELECT ...) WHERE ...`

## 实现要点

| 模块 | 内容 |
|------|------|
| `parse_defs.h` / `yacc_sql.y` | `UpdateSqlNode::value_expr`；`SET col = expression` |
| `update_stmt.cpp` | `ExpressionBinder` 绑定 SET 右值 |
| `update_logical/physical_operator` | 持有 `unique_ptr<Expression>`，逐行求值 |
| `physical_plan_generator` | `prepare/open/close_subquery_expressions` |
| `update_physical_operator` | 逐行 `get_value` + 类型转换/NULL 检查；失败时逆序 `trx_->update_record` 回滚 |

## 自测

```bash
bash build.sh debug --make -j8
python3 lab/test/run_all.py --only custom-update-select,custom-update,official-update
python3 lab/test/run_all.py --quick   # 33/33 PASS
```

| 用例 | 结果 |
|------|------|
| `custom-update-select` | PASS（含多行标量失败、unique 回滚） |
| `official-update` / `custom-update` | PASS |
| 全量 `--quick` | **33/33** |

## 相关文件

- 自测 SQL：`lab/test/cases/sql/update-select.sql`
- manifest：`custom-update-select`
