# MiniOB 下一阶段最值得做的 8 个任务

目标：基于当前已完成（basic/date/drop-table/update/aggregation/like/join/function/order-by）状态，挑选“分值高 + 依赖关键 + 通过概率高 + 覆盖面广”的 8 个任务，作为后续主线。

---

## 0. 优先级总览（建议执行顺序）

1. `null`（3分，基础能力，影响大量题）
2. `group-by`（4分，和已做 aggregation/order-by 结合紧密）
3. `simple-sub-query`（4分，中高收益，后续复杂子查询前置）
4. `multi-index`（4分，索引能力升级）
5. `update-select`（4分，子查询 + DML）
6. `complex-sub-query`（5分，困难题，建议至少拿下一道）
7. `big-query`（5分，通常“前面功能做对就能过”）
8. `big-write`（5分，同上，重稳健性）

> 说明：`big-order-by` 也是 5 分，但你已经做了 `order-by`，后续可以作为第 9 个冲分项补上。

---

## 1) null（3 分）

### 题目价值
- 是后续很多题（sub-query/group-by/having/update-select）的语义基础。
- 官方提示明确：NULL 题“非常基础，出现在许多其它用例中”。

### 功能边界
- 列定义支持 `NULL` / `NOT NULL`。
- 插入/更新时校验非空约束。
- `IS NULL` / `IS NOT NULL`。
- 三值逻辑关键点：`NULL` 与普通比较（`= <> < >`）结果按题目预期处理。

### 建议改动点
- 解析层：`src/observer/sql/parser/lex_sql.l`、`src/observer/sql/parser/yacc_sql.y`、`src/observer/sql/parser/parse_defs.h`
- 元信息：`src/observer/storage/field/field_meta.h/.cpp`（增加 nullable）
- 表元信息序列化：`src/observer/storage/table/table_meta.cpp`
- 记录存储：`src/observer/storage/table/table.cpp`、`src/observer/storage/record/*`（null bitmap）
- 比较与过滤：`src/observer/sql/expr/expression.cpp`、`src/observer/sql/stmt/filter_stmt.cpp`

### 实施步骤
1. 扩展字段元数据：增加 nullable 标记并持久化。
2. 在 record 中引入 null bitmap（每列一位）。
3. `INSERT/UPDATE` 时对 NOT NULL 列做约束校验。
4. 比较表达式增加 NULL 语义；补 `IS NULL/IS NOT NULL`。
5. 索引路径验证（含 nullable 列时的行为一致性）。

### 测试建议
- 官方：`test/case/test/primary-null.test`
- 自测覆盖：
  - `NOT NULL` 列插入 `NULL` 是否失败
  - `IS NULL/IS NOT NULL`
  - `NULL` 与常量、列比较
  - 带索引列出现 NULL 的查询

### 风险与规避
- 风险：只在表达式层处理 NULL，而存储层没位图，导致重启后语义错。
- 规避：先打通“元数据 + 存储 + 执行”闭环，再补语义细节。

---

## 2) group-by（4 分）

### 题目价值
- 与你已完成的 aggregation / order-by 高度耦合，改动边际低、得分高。
- 完成后可为 sub-query / view 类题提供能力底座。

### 功能边界
- 多字段 `GROUP BY`
- 聚合函数：count/min/max/avg（你已有聚合基础）
- `HAVING`（官方要求）
- 分组字段包含 NULL 的场景

### 建议改动点
- 解析：`lex_sql.l`、`yacc_sql.y`（增加 `having`）
- 语句绑定：`select_stmt.cpp/.h`
- 逻辑计划：`logical_plan_generator.cpp`（已有 group by 骨架可扩）
- 物理算子：`group_by_physical_operator.cpp`、`hash_group_by_physical_operator.cpp`

### 实施步骤
1. 扩展语法：`SELECT ... GROUP BY ... HAVING ...`。
2. 将 having 条件绑定为表达式（聚合后再过滤）。
3. 执行计划顺序：`... -> GroupBy -> Having(Predicate) -> Project`。
4. 对 NULL 分组键做一致性处理（同值归组）。

### 测试建议
- 官方：`test/case/test/primary-group-by.test`
- 自测：
  - 单字段、多字段分组
  - where + group by
  - having 过滤
  - 多表 join 后 group by

### 风险与规避
- 风险：把 having 当 where 提前执行，语义错误。
- 规避：严格放在聚合之后。

---

## 3) simple-sub-query（4 分）

### 题目价值
- 分值高，且是 `complex-sub-query` 的必经前置。
- 能快速提高 SQL 能力完整度（IN/NOT IN/EXISTS/标量子查询）。

### 功能边界
- `IN / NOT IN` 子查询
- `EXISTS / NOT EXISTS`
- 标量比较子查询（单值）
- 子查询含聚合函数
- 非关联子查询（简单子查询定义）

### 建议改动点
- 解析：`parse_defs.h`、`yacc_sql.y`（子查询表达式节点）
- 表达式：`src/observer/sql/expr/expression.h/.cpp`
- 绑定：`expression_binder.cpp`
- 计划：`logical_plan_generator.cpp`、`physical_plan_generator.cpp`

### 实施步骤
1. 先实现非关联 `IN/NOT IN`。
2. 再实现标量子查询比较。
3. 最后补 `EXISTS/NOT EXISTS`。
4. 增加错误分支：多行子查询参与单值比较时报错。

### 测试建议
- 官方：`test/case/test/primary-simple-sub-query.test`
- 自测：
  - 空结果子查询
  - 聚合子查询
  - 错误用法（`select *` 作为标量子查询）

### 风险与规避
- 风险：子查询执行重复过多，性能差。
- 规避：非关联子查询结果可缓存一次。

---

## 4) multi-index（4 分）

### 题目价值
- 索引层核心升级，分值高且和 unique 直接相关。
- 后续查询性能与稳定性都有帮助。

### 功能边界
- `create index idx on t(c1,c2,...)`
- 建索引、插入、删除、查询路径一致
- 复合键比较顺序正确

### 建议改动点
- 解析：`yacc_sql.y`（索引列列表）
- 元数据：`storage/index/index_meta.h/.cpp`（从单字段到多字段）
- 表元信息：`table_meta.cpp`
- 索引键编码：`storage/index/bplus_tree.*`
- DML 同步：`table.cpp`、索引维护路径

### 实施步骤
1. 元数据先支持多字段数组并可序列化。
2. 复合键拼接编码（按字段顺序）。
3. B+Tree 比较器按字典序比较多列键。
4. 插入/删除回写所有相关复合索引。

### 测试建议
- 官方：`test/case/test/primary-multi-index.test`
- 自测：
  - 空表建索引/非空表建索引
  - 插入删除后索引一致性
  - 复合条件查询正确性

### 风险与规避
- 风险：只改了建索引，忘记 DML 维护。
- 规避：每条 insert/delete/update 后加断言或核对查询结果。

---

## 5) update-select（4 分）

### 题目价值
- 把查询能力和更新能力串起来，难度适中、分值可观。
- 能顺带促进 `update-mvcc` 的准备。

### 功能边界
- `update t set c = (select ...) where ...`
- 子查询可能是普通查询或聚合查询
- 失败回滚（至少语句级一致性）

### 建议改动点
- 解析：`yacc_sql.y`、`parse_defs.h`（update value 支持表达式）
- 语句：`update_stmt.cpp/.h`
- 执行：`update_physical_operator.cpp` 或 executor 路径
- 事务回滚路径：`table.cpp`、trx 相关逻辑

### 实施步骤
1. update 的右值从字面量扩展为表达式/子查询。
2. 对每个命中行执行表达式求值。
3. 子查询结果合法性检查（单值约束）。
4. 多行更新遇到失败时做回滚策略。

### 测试建议
- 官方：优先查看 update-select 对应用例（若本地无，按题目构造）
- 自测：
  - 聚合子查询赋值
  - 空结果子查询
  - 多行更新 + 中途失败

### 风险与规避
- 风险：逐行更新时子查询重复执行，且失败回滚不完整。
- 规避：先做“全求值后批量写入”再做性能优化。

---

## 6) complex-sub-query（5 分，困难题）

### 题目价值
- 5 分高价值，同时满足“至少一道困难题”的要求。
- 做完对答辩说服力极强。

### 功能边界
- 关联子查询（子查询依赖外层行）
- 子查询中再嵌套子查询
- 子查询中出现聚合

### 建议改动点
- 表达式执行层：新增关联上下文（outer tuple）
- 绑定层：区分内外层作用域
- 计划层：支持子查询节点引用外层列

### 实施步骤
1. 在 simple-sub-query 基础上扩展“外层列引用”。
2. 先支持一层关联，再支持嵌套关联。
3. 明确非法语句判定（单独执行关联子查询应报错）。

### 测试建议
- 官方：`test/case/test/primary-complex-sub-query.test`
- 自测：
  - `where x in (select ... where inner.a = outer.a)`
  - 聚合 + 关联混合
  - 空表/空子查询

### 风险与规避
- 风险：作用域绑定混乱，字段解析到错误层级。
- 规避：Binder 增加显式 scope 栈，逐层查找。

---

## 7) big-query（5 分）

### 题目价值
- 5 分且官方提示“前面功能做对，通常可通过”。
- 投入产出比很高，适合中后期冲分。

### 功能边界
- 大数据量插入、更新、查询
- 多表（1~3 表）随机抽查正确性

### 建议改动点
- 主要是稳健性：记录管理、索引维护、执行器循环逻辑
- 关注内存增长：排序/聚合/子查询缓存策略

### 实施步骤
1. 先确保已有题目在较大数据下无逻辑错误。
2. 跑批量插入更新脚本观察耗时与内存。
3. 定位慢点：扫描、谓词、排序、索引命中。

### 测试建议
- 若仓库无 `primary-big-query.test`，自建压力 SQL：
  - 1~3 表，每表几千行
  - 批量 update + 随机 select 校验

### 风险与规避
- 风险：功能正确但性能抖动导致超时。
- 规避：优先保证索引路径可用，避免全表重复扫描。

---

## 8) big-write（5 分）

### 题目价值
- 同为 5 分，且与 big-query 共享大量基础。
- 主要考察“长期写入 + 更新 + 删除”的一致性。

### 功能边界
- 大量随机增删改查
- 长时间执行稳定性（不能崩、不能错）

### 建议改动点
- 事务/日志边界（即使是简化事务）
- 索引同步完整性（尤其 delete/update）
- 记录回收与页面管理稳定性

### 实施步骤
1. 先跑 deterministic 数据脚本（可复现）。
2. 再跑随机脚本（固定 seed）。
3. 每轮操作后做校验查询（计数、主键集合、一致性）。

### 测试建议
- 自建 `big_write.sql`：
  - 批量 insert
  - 随机 update/delete
  - 最终全量校验

### 风险与规避
- 风险：删除后索引残留、更新后旧键未清理。
- 规避：把“先删旧索引键、再写新键”的顺序固化并覆盖测试。

---

## 建议落地节奏（两周版）

- 第 1 周：`null -> group-by -> simple-sub-query -> multi-index`
- 第 2 周：`update-select -> complex-sub-query -> big-query -> big-write`

每天固定流程：
1. 改动不超过 2~4 个核心文件一组；
2. `bash build.sh debug --make -j8`；
3. 先跑对应 primary 用例，再跑你自己的 `yys/test/*.sql`；
4. 当天写开发日志（保持“可追溯”）。

---

## 附：本文件的定位

这个文件是“任务规划与实施说明”总文档。  
如果你希望，我下一步可以继续在 `lab/todo` 里按这 8 题再拆成 8 个独立执行清单（每题一个 checklist 文件，直接用于开发打勾）。

