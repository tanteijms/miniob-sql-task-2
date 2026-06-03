# update-select 开发日志

## 阶段信息

- 日期：2026-06-03
- 负责人：XC 工作区
- 当前阶段：`5) update-select` 主链接管与边界收口
- 协作约定：本轮不等待本机编译与测试，编译、联调、回归交由同伴继续推进

## 目标范围

本阶段目标是收口 `UPDATE t SET c = (SELECT ...) WHERE ...`：

- `UPDATE SET` 右值支持字面量、普通表达式、标量子查询
- 复用当前仓库已有 `UnboundSubQueryExpr` / `SubQueryExpr` / `InSubQueryExpr` 体系
- 保留 `UPDATE WHERE` 的旧 `ConditionSqlNode -> FilterStmt` 路线，不扩展 WHERE 子查询
- 多行标量子查询报错
- 更新过程中失败时回滚已写入记录，避免语句半成功

## 已参考资料

- 任务说明：`lab/todo/top8-最值得做的任务.md`
- 官方 update 回归：`test/case/test/primary-update.test`
- 自测 SQL：`lab/test/cases/sql/update-select.sql`
- 既有参考日志：`lab/log/yys-dev/18开发日志-update-select.md`

## 当前代码主链状态

经静态检查，当前仓库中 `update-select` 已有较完整半成品：

1. Parser / AST
   - `yacc_sql.y` 中 `update_stmt` 已为 `UPDATE ID SET ID EQ expression where`
   - `UpdateSqlNode` 已用 `unique_ptr<Expression> value_expr` 承载 SET 右值
   - `UPDATE WHERE` 仍使用旧 `where -> condition_list`
2. Binder / Stmt
   - `UpdateStmt::create` 已校验目标表与目标字段
   - SET 右值已通过 `ExpressionBinder` 绑定
   - binder context 只加入被更新表，并向 `ExpressionBinder` 传入 `Db *`
3. Logical / Physical Plan
   - `LogicalPlanGenerator` 已将 `UpdateStmt::value_expr()` 移入 `UpdateLogicalOperator`
   - `PhysicalPlanGenerator` 已在创建 `UpdatePhysicalOperator` 前调用 `prepare_subquery_expressions`
4. Execution
   - `UpdatePhysicalOperator` 已先扫描命中行并缓存旧记录
   - 每行通过 `value_expr_->get_value(row_tuple, value)` 计算 SET 右值
   - 写入前处理类型转换和 `NULL / NOT NULL` 校验
   - 已有逆序 `rollback_updates()`，用于失败时回滚已更新记录

## 本轮收口改动

1. 补齐子查询表达式复制路径
   - `SubQueryExpr::copy()` 不再直接返回 `nullptr`
   - `InSubQueryExpr::copy()` 不再直接返回 `nullptr`
   - 复制体保留表达式元结构，避免表达式复制/重写路径遇到子查询节点时直接空指针失败
2. 保持执行计划生成职责不变
   - 复制体不伪造内部 `SelectStmt` 或物理计划
   - 真正执行仍依赖原始 binder / `prepare_subquery_expressions` / `open_subquery_expressions` 主链
3. 未扩展范围
   - 未修改 `UPDATE WHERE`
   - 未改空标量子查询语义
   - 未新增本地编译或测试要求

## 待同伴编译/调试关注点

1. `SubQueryExpr::copy()` 当前是安全复制体，不是完整深拷贝内部 `SelectStmt`
2. 空标量子查询当前按现有 `SubQueryExpr` 行为处理，若官方 update-select 要求赋 `NULL`，需在后续调试中确认后再改
3. 关联 update-select 依赖当前 outer tuple 栈机制：
   - `UPDATE us_t1 SET val = (SELECT score FROM us_t2 WHERE us_t2.id = us_t1.id)`
4. 需要重点跑：
   - `custom-update-select`
   - `official-update`
   - 原 `primary-update.test`

## 当前结论

- `update-select` 主链在当前仓库中已经基本成型
- 本轮已补上表达式复制路径的关键空指针缺口
- 代码状态适合交由同伴继续编译、联调、回归
