# simple-sub-query 开发日志

## 阶段信息

- 日期：2026-06-03
- 负责人：XC 工作区
- 当前阶段：`3) simple-sub-query` 的前置分析与低冲突实现准备
- 协作约束：`1) null` 正由同伴并行开发，当前需避免覆盖其在 parser / value / field_meta 热区的修改

## 官方用例梳理

主用例文件：

- `test/case/test/primary-simple-sub-query.test`

当前已确认的官方覆盖范围：

1. 非关联 `IN (sub query)`
2. 非关联 `NOT IN (sub query)`
3. 聚合子查询作为标量参与比较
4. 左右交换位置的标量子查询比较
5. 空结果子查询
6. 非法用法报错：
   - 多行结果用于标量比较
   - `select *` 用作标量子查询
   - `select *` 用作 `IN/NOT IN` 子查询

## 与 null 阶段的冲突面

当前仓库中已观测到未提交修改：

- `src/observer/common/value.cpp`
- `src/observer/common/value.h`
- `src/observer/sql/parser/lex_sql.l`
- `src/observer/sql/parser/parse_defs.h`
- `src/observer/sql/parser/yacc_sql.y`
- `src/observer/sql/stmt/insert_stmt.cpp`
- `src/observer/sql/stmt/update_stmt.cpp`
- `src/observer/storage/field/field_meta.cpp`
- `src/observer/storage/field/field_meta.h`

结论：

- parser 层与 `Value` 语义层均属于共享高风险区域
- 本阶段优先完成子查询方案、影响链梳理、执行链承载设计
- 代码落地时优先选择低冲突文件，待 `null` 合并后再补 parser / expr 交叉区

## 影响链溯源

当前已阅读和确认的关键链路：

1. 语法结构定义
   - `src/observer/sql/parser/parse_defs.h`
2. 表达式绑定
   - `src/observer/sql/parser/expression_binder.cpp`
   - `src/observer/sql/expr/expression.h`
   - `src/observer/sql/expr/expression.cpp`
3. where / join 条件承载
   - `src/observer/sql/stmt/filter_stmt.h`
   - `src/observer/sql/stmt/filter_stmt.cpp`
4. select 语义构建
   - `src/observer/sql/stmt/select_stmt.h`
   - `src/observer/sql/stmt/select_stmt.cpp`
5. 逻辑计划生成
   - `src/observer/sql/optimizer/logical_plan_generator.cpp`

## 当前设计判断

`simple-sub-query` 不能只在 `FilterStmt` 层硬塞，因为现有 `ConditionSqlNode -> FilterStmt -> ComparisonExpr` 路径只支持 “字段/常量” 二元比较，无法承载：

- 子查询结果集
- 标量子查询单值
- `IN/NOT IN` 集合语义
- 多行标量子查询报错

因此后续大概率需要：

1. 在表达式层新增子查询相关表达式节点
2. 在 binder 中对子查询表达式做独立绑定
3. 在 `SelectStmt` / 计划生成阶段为子查询生成独立的 `Stmt/Plan`
4. 在执行阶段为非关联子查询做一次性求值与缓存

## 本阶段可并行推进的内容

### 可先做

1. 明确子查询表达式的数据结构与生命周期
2. 设计非关联子查询的一次性求值与缓存接口
3. 梳理 `SelectStmt -> LogicalPlan -> PhysicalPlan` 的承载方式
4. 识别并预留错误码与错误分支位置

### 暂缓

1. `lex_sql.l`
2. `yacc_sql.y`
3. `parse_defs.h`
4. `expression.h / expression.cpp` 中与 `null` 可能重叠的比较语义改动

## 预期实现顺序

1. 先完成非关联 `IN/NOT IN`
2. 再完成标量子查询比较
3. 再补空结果行为与多行错误分支
4. 最后视 `null` 合并情况补 parser / comparison 语义联动

## 本阶段交付标准

- 方案明确
- 修改范围最小化
- Docker 内 `cmake --build build -j4` 编译通过
- 测试交由同伴异步执行

## 当前进度

- 已完成官方用例梳理
- 已完成共享冲突面识别
- 已开始追踪 `select / filter / binder / expr / logical plan` 链路
- 已确认一个设计级阻塞：`simple-sub-query` 的最小可运行闭环无法完全避开 `null` 热区
- 重新检查后确认：`null` 的基础设施已经基本在当前代码树中可见，因此已恢复 `simple-sub-query` 主线开发

## 当前阻塞与原因

### 阻塞结论

经过继续阅读 `FilterStmt`、`SelectStmt`、`LogicalPlanGenerator`、`PhysicalPlanGenerator` 与 `yacc_sql.y`，确认：

- 当前 `WHERE` 条件仍走 `ConditionSqlNode -> FilterStmt -> ComparisonExpr` 老路径
- `ConditionSqlNode` 左右两侧仅支持：
  - 字段
  - 常量
- 因此子查询结果目前无法被 parser 与语义层表达，更无法进入执行计划

### 直接影响

这意味着以下文件不是“后补优化”，而是 `simple-sub-query` 的最小必要改动：

1. `src/observer/sql/parser/yacc_sql.y`
2. `src/observer/sql/parser/parse_defs.h`
3. `src/observer/sql/expr/expression.h`
4. `src/observer/sql/expr/expression.cpp`
5. `src/observer/sql/parser/expression_binder.cpp`

而其中前四项与同伴 `null` 开发存在明显冲突风险。

### 当前处理原则

依据 AGENTS 协议，在大型多模块联动任务中，出现设计阻塞后不应继续盲目推进代码修改。当前先暂停正式编码，等待与开发者对齐以下策略之一：

1. 等同伴先提交 `null` 第一版，我们再基于其结果接入 `simple-sub-query`
2. 允许当前阶段直接进入共享热区开发，后续通过人工合并解决冲突
3. 由同伴暂时避开 parser / expr，我们接管 `simple-sub-query` 主线实现

## 本轮实际代码改动

### 已完成

1. `SELECT` 的 `WHERE` 入口已开始从旧 `ConditionSqlNode` 条件链切向表达式链
2. `SelectSqlNode` 中新增：
   - `where_conditions`
3. `SelectStmt` 中新增：
   - `where_expression_`
   - `where_expression()` 访问接口
4. `SelectStmt::create` 已改为：
   - 对 `select_sql.where_conditions` 做 expression binder 绑定
   - 将多个 where 条件组装成 `ConjunctionExpr(AND)`
   - 不再为 `SELECT WHERE` 构造旧 `FilterStmt`
5. `LogicalPlanGenerator::create_plan(SelectStmt *)` 已改为：
   - 优先使用 `where_expression()` 构造 `PredicateLogicalOperator`
6. `yacc_sql.y` 已做第一轮拆分：
   - `SELECT ... WHERE ...` 走新的表达式型 `select_where`
   - `DELETE/UPDATE` 继续沿用旧的 `where -> condition_list`
   - 已加入 `where_condition` / `where_condition_list`
   - 目前支持：
     - `expression comp_op expression`
     - `expression comp_op value`
     - `expression IS NULL`
     - `expression IS NOT NULL`

### 当前尚未完成

1. 还没有正式接入子查询表达式节点
2. 还没有把 `(select ...)` 作为 `expression` 的一种语法形式接入 parser
3. 还没有在 binder 中绑定子查询内部 `SelectStmt`
4. 还没有实现非关联子查询的一次性求值与缓存
5. 还没有补上官方用例中的：
   - `IN / NOT IN (sub query)`
   - 标量子查询比较
   - 多行标量子查询报错
   - `select *` 子查询报错

## 本轮编译情况

### 已观察到的结果

1. 第一次阶段性编译发现 parser 类型串扰问题：
   - `DELETE/UPDATE` 误吃到了 `SELECT` 新的 `WHERE` 表达式列表
   - 现已通过将 `select_where` 与旧 `where` 分离的方式修正
2. 第二次编译未立即暴露新的同类 parser 错误，但由于耗时较长未在本轮继续等待完毕

### 本轮处理结论

- 按开发者最新要求，本轮**不继续等待完整编译结果**
- 当前日志如实记录为：
  - “代码已推进到 `SELECT WHERE` 表达式化阶段”
  - “parser 第一处类型串扰已修复”
  - “尚未完成最终编译闭环确认”

## 下一步建议

1. 先继续把子查询表达式节点与 parser 入口补齐
2. 再接 binder / stmt / 执行期求值
3. 待这一版功能主链更完整后，再统一做一次 Docker 编译确认
