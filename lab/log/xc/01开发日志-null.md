# MiniOB 课程设计开发日志 - null

> 记录时间：2026-06-03
> 阶段：null
> 工作区：xc
> 当前状态：进入实现中，已完成官方用例与影响链分析；测试由同伴异步执行，本地以“完整编译通过”为硬指标

---

## 1. 任务说明

- 官方主验收用例：`test/case/test/primary-null.test`
- 目标能力：
  - 列定义支持 `NULL / NOT NULL`
  - 插入与更新时校验非空约束
  - 支持 `IS NULL / IS NOT NULL`
  - 普通比较在任一侧为 `NULL` 时按 unknown 语义落为过滤失败
  - 聚合函数对 `NULL` 做跳过处理
  - 索引列为 `NULL` 时保持索引路径一致性

## 2. 设计决策

- 采用方案 A：`row record` 增加 `null bitmap`
- bitmap 仅覆盖用户列，不覆盖系统事务列
- 普通字段 offset 整体后移，record 布局调整为：
  - `[null bitmap][field payloads...]`
- 旧表文件与旧元数据文件不做兼容，验证前默认清空旧数据库目录
- 单列索引遇到 `NULL` 键时跳过建索引项，不单独设计空键编码

## 3. 影响链

- Parser：
  - `lex_sql.l`
  - `yacc_sql.y`
  - `parse_defs.h`
- Value / Expr：
  - `common/value.*`
  - `sql/expr/expression.*`
  - `sql/expr/aggregator.*`
- Meta / Storage：
  - `storage/field/field_meta.*`
  - `storage/table/table_meta.*`
  - `storage/table/table.*`
  - `sql/expr/tuple.h`
- Index：
  - `storage/index/index.*`
  - `storage/index/bplus_tree_index.cpp`

## 4. 实施分层

1. 先打通 parser 与 value 的 NULL 表达
2. 再落 `FieldMeta/TableMeta` 的 nullable 与 bitmap 布局
3. 再补 `Table/RowTuple` 的写 NULL、读 NULL
4. 最后补比较表达式、聚合器、索引与结果输出

## 5. 编译与测试约定

- 编译命令：
  - `docker exec miniob-dev bash -lc "cd /miniob && cmake --build build -j4"`
- 当前协定下不在本地执行全量测试脚本
- 交付口径：
  - 已通过全量编译，等待同伴测试对接

## 6. 修订记录

- 2026-06-03：完成正式开工前分析并进入实现
- 2026-06-03：已补充手工验证脚本 `lab/test/cases/sql/null.sql`，供同伴按 `primary-null.test` 主线做 SQL 冒烟与回归对照。
- 2026-06-03：测试前需先清空旧数据库目录；由于 row record 布局已改为带 null bitmap，旧表文件不兼容。
- 2026-06-03：已在 `miniob-dev` 容器内通过自写 Python harness 直连 `observer` 做主线 SQL 验证，发现当前运行二进制对 `NULL` 主线仍未达可测状态：
  - `create table ... int null/not null` 返回 `SQL_SYNTAX > Failed to parse sql`
  - `insert/update ... null`、`expr is null`、`expr is not null` 返回同类语法错误
  - 依赖建表成功的后续 `select/aggregate/join` 多数返回 `FAILURE`
  - 说明至少存在“parser 改动未真正进入当前运行二进制”或“相关语法/语义实现仍不完整”之一
- 2026-06-03：根据单日志锁原则，当前先停止继续宣称可测，需先修复上述解析/运行问题后再进入下一轮验证。
