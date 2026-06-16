# MiniOB 仓库上下文索引（给 Codex / AI 助手）

> **用法**：处理本仓库任务时，**先读本文**，再按「任务类型 → 必读文件」按需打开源码；**不要**全量扫描 `src/`、`test/`、`docs/`。  
> 课程设计正文见 [`数据库课程设计实验报告-完整版.md`](./数据库课程设计实验报告-完整版.md)；协作规范见仓库根目录 [`AGENTS.md`](../../AGENTS.md)。

---

## 1. 项目是什么（30 秒）

- **MiniOB**：OceanBase 社区维护的 C++ 教学数据库内核（[官方文档](https://oceanbase.github.io/miniob/)）。
- **本仓库任务**：在北邮课程设计指导书 25 道题基础上做 SQL 能力扩展，**不是从零写数据库**。
- **当前进度**：**18 / 25 题已实现**，135 条自动化用例 100% 通过（见 [`lab/test/latest.md`](../test/latest.md)）。
- **类比**：与编译原理课类似——词法/语法 → 语义分析 → 中间表示（表达式/算子树）→ 代码生成/执行 → 存储。

---

## 2. SQL 执行主链（必读概念）

```text
客户端 SQL 文本
  → ParseStage      lex_sql.l / yacc_sql.y → ParsedSqlNode
  → ResolveStage    Stmt::create + ExpressionBinder → *Stmt
  → OptimizeStage   LogicalPlanGenerator → PhysicalPlanGenerator
  → ExecuteStage    PhysicalOperator 火山模型 或 CommandExecutor（DDL）
  → Storage         HeapTableEngine / B+Tree / RecordManager
```

**路径分叉（重要）**

| 语句类型 | 执行路径 | 入口 |
|----------|----------|------|
| DDL（DROP TABLE、CREATE INDEX 等） | `CommandExecutor` → `Db`/`Table` | `execute_stage.cpp` 无 physical_operator 分支 |
| SELECT、复杂 UPDATE | 逻辑/物理算子树 | `execute_stage.cpp` → `SqlResult::set_operator` |

**计划生成顺序（SELECT）**：`Scan → WHERE(Predicate) → Join → GroupBy → HAVING → Sort → Project`

**设计原则（改代码前对齐）**

1. Parser 只造语法结构，语义校验在 Stmt/Binder。
2. 查询走算子链，不在 CommandExecutor 里手写扫表。
3. 函数/LIKE/NULL/子查询统一为 `Expression`，WHERE/HAVING/UPDATE SET 共用。
4. DML 与全部索引同步；失败按步骤回滚。

---

## 3. 题目完成情况

| 状态 | 题目 |
|------|------|
| **已实现（18）** | basic, date, drop-table, update, aggregation-func, like, join-tables, simple-sub-query, function, multi-index, unique, null, update-select, expression, order-by, group-by, complex-sub-query, big-query, big-write |
| **未实现（7）** | alias, text, create-view, create-table-select, update-mvcc, big-order-by |

分值与验收要求：[`lab/题目/指导书.md`](../题目/指导书.md) 第三章 + 末尾验收表。

---

## 4. 分层 → 核心文件（按层读，不要扫全目录）

### 4.1 入口与阶段调度

| 文件 | 作用 |
|------|------|
| `src/observer/sql/parser/parse_stage.cpp` | 词法语法入口 |
| `src/observer/sql/parser/resolve_stage.cpp` | Stmt 创建 |
| `src/observer/sql/optimizer/optimize_stage.cpp` | 计划生成 |
| `src/observer/sql/executor/execute_stage.cpp` | DDL vs 算子执行分叉 |
| `src/observer/sql/executor/command_executor.cpp` | DDL 分发 |

### 4.2 解析层（改语法 / 关键字时读）

| 文件 | 作用 |
|------|------|
| `src/observer/sql/parser/lex_sql.l` | 词法（**改关键字从这里**） |
| `src/observer/sql/parser/yacc_sql.y` | 语法（**改产生式从这里**） |
| `src/observer/sql/parser/parse_defs.h` | AST / SqlNode 结构定义 |
| `src/observer/sql/parser/parse.cpp` | 解析辅助 |

> 注：`lex_sql.cpp`、`yacc_sql.cpp` 为生成物；优先改 `.l`/`.y`。

### 4.3 语义绑定层

| 文件 | 作用 |
|------|------|
| `src/observer/sql/stmt/stmt.cpp` | `Stmt::create_stmt` 总分发 |
| `src/observer/sql/stmt/select_stmt.cpp` | SELECT 绑定 |
| `src/observer/sql/stmt/update_stmt.cpp` | UPDATE 绑定（含 SET 表达式） |
| `src/observer/sql/stmt/drop_table_stmt.cpp` | DROP TABLE |
| `src/observer/sql/stmt/create_index_stmt.cpp` | CREATE INDEX / UNIQUE |
| `src/observer/sql/parser/expression_binder.cpp` | **表达式与子查询绑定核心** |
| `src/observer/sql/parser/expression_binder.h` | Binder 上下文、outer_ref |

### 4.4 表达式系统（多题共用，优先理解）

| 文件 | 作用 |
|------|------|
| `src/observer/sql/expr/expression.h` | Expression 类型枚举与接口 |
| `src/observer/sql/expr/expression.cpp` | 比较、求值、NULL 语义 |
| `src/observer/sql/expr/subquery_expr.h/.cpp` | 标量 / IN 子查询 |
| `src/observer/sql/expr/aggregator.cpp` | 聚合 COUNT/MIN/MAX/AVG/SUM |
| `src/observer/sql/expr/sql_function.cpp` | length / round / date_format |
| `src/observer/sql/expr/like_match.cpp` | LIKE `%` `_` |
| `src/observer/sql/expr/tuple.h` | 行/tuple、字段取值 |
| `src/observer/common/value.h/.cpp` | Value、`is_null()` |

### 4.5 计划与算子层

| 文件 | 作用 |
|------|------|
| `src/observer/sql/optimizer/logical_plan_generator.cpp` | 逻辑算子树 |
| `src/observer/sql/optimizer/physical_plan_generator.cpp` | 物理算子实例化 |
| `src/observer/sql/operator/table_scan_physical_operator.cpp` | 全表扫描 |
| `src/observer/sql/operator/predicate_physical_operator.cpp` | WHERE 过滤 |
| `src/observer/sql/operator/nested_loop_join_physical_operator.cpp` | INNER JOIN（NLJ） |
| `src/observer/sql/operator/group_by_physical_operator.cpp` | GROUP BY |
| `src/observer/sql/operator/scalar_group_by_physical_operator.cpp` | 无 GROUP BY 列的聚合 |
| `src/observer/sql/operator/sort_physical_operator.cpp` | ORDER BY（内存排序） |
| `src/observer/sql/operator/update_physical_operator.cpp` | UPDATE 执行 + 回滚 |
| `src/observer/sql/operator/update_logical_operator.cpp` | UPDATE 逻辑算子 |

优化器（本课题改动少，除非做规则扩展）：`predicate_pushdown_rewriter.cpp`、`rewriter.cpp`；`optimizer/cascade/` 为框架代码，**默认不读**。

### 4.6 存储与索引层

| 文件 | 作用 |
|------|------|
| `src/observer/storage/db/db.cpp` | `drop_table` 等 catalog |
| `src/observer/storage/table/table.cpp` | 建表、make_record、读写 |
| `src/observer/storage/table/table_meta.cpp` | **null bitmap**、`field_is_null` |
| `src/observer/storage/table/heap_table_engine.cpp` | INSERT/UPDATE/DELETE + 索引维护 |
| `src/observer/storage/field/field_meta.cpp` | `nullable_` 字段属性 |
| `src/observer/storage/index/index_meta.cpp` | 复合索引、unique 标志 |
| `src/observer/storage/index/bplus_tree_index.cpp` | 插入查重、键编码 |
| `src/observer/storage/index/bplus_tree.cpp` | B+ 树（深读索引题时才开） |
| `src/observer/storage/record/record_manager.cpp` | 页内记录布局 |
| `src/observer/common/type/date_type.cpp` | DATE 校验与编码 |

### 4.7 DDL Executor（改 DROP/INDEX 时读）

| 文件 | 作用 |
|------|------|
| `src/observer/sql/executor/drop_table_executor.cpp` | DROP TABLE 执行 |
| `src/observer/sql/executor/create_index_executor.cpp` | CREATE INDEX 执行 |

---

## 5. 按任务类型：最小阅读清单

> 开发某题时：**先读测试用例 → 再读下表文件**；单文件过长则读相关函数 ±50 行即可。

| 任务 / 题目 | 先读测试 | 再读源码（按顺序） |
|-------------|----------|-------------------|
| **任意新题** | 官方 `test/case/test/primary-*.test` 或 `lab/test/cases/sql/*.sql` | `AGENTS.md` §1 → 本表对应行 |
| drop-table | `primary-drop-table.test`, `lab/test/cases/sql/drop-table.sql` | `yacc_sql.y` → `drop_table_stmt.cpp` → `drop_table_executor.cpp` → `db.cpp` |
| update | `primary-update.test`, `update.sql`, `update-index.sql` | `update_stmt.cpp` → `logical/physical_plan_generator.cpp` → `update_physical_operator.cpp` → `heap_table_engine.cpp` |
| date | `primary-date.test`, `date.sql` | `date_type.cpp` → `yacc_sql.y` → `table.cpp`（make_record） |
| null | `primary-null.test`, `null.sql` | `field_meta.cpp` → `table_meta.cpp` → `expression.cpp`（compare）→ `aggregator.cpp` |
| like | `like.sql`, comprehensive like 用例 | `lex_sql.l` → `expression.cpp` → `like_match.cpp` |
| function / expression | `primary-expression.test`, `function.sql` | `yacc_sql.y` → `expression_binder.cpp` → `sql_function.cpp` → `expression.cpp` |
| join-tables | `primary-join-tables.test`, `join.sql` | `yacc_sql.y` → `select_stmt.cpp` → `logical_plan_generator.cpp` → `nested_loop_join_physical_operator.cpp` |
| order-by | `primary-order-by.test`, `order-by.sql` | `yacc_sql.y` → `logical_plan_generator.cpp` → `sort_physical_operator.cpp` |
| group-by | `primary-group-by.test`, `group-by.sql` | `logical_plan_generator.cpp` → `group_by_*` → `aggregator.cpp` |
| multi-index | `primary-multi-index.test`, `multi-index.sql` | `create_index_stmt.cpp` → `index_meta.cpp` → `bplus_tree_index.cpp` → `heap_table_engine.cpp` |
| unique | `primary-unique.test`, `unique.sql` | 同上 + `bplus_tree_index.cpp` insert 查重 |
| simple-sub-query | `primary-simple-sub-query.test`, `simple-sub-query.sql` | `yacc_sql.y`（WHERE 表达式）→ `expression_binder.cpp` → `subquery_expr.cpp` → `predicate_physical_operator.cpp` |
| complex-sub-query | `primary-complex-sub-query.test`, `complex-sub-query.sql` | 在上基础上 + `expression_binder.cpp`（outer_ref）→ `subquery_expr.cpp`（correlated） |
| update-select | `update-select.sql`, comp-097~100 | `update_stmt.cpp` → `update_physical_operator.cpp` → 复用 `subquery_expr.cpp` |
| big-query / big-write | `lab/test/stress/gen_big_*.py` 生成 SQL | `heap_table_engine.cpp`（DML+索引）+ 各算子；日志见 `lab/log/xc/07*.md`、`yys-dev/12*.md` |
| **写实验报告** | — | [`数据库课程设计实验报告-完整版.md`](./数据库课程设计实验报告-完整版.md) + `lab/test/latest.md` + 对应 `lab/log/*/` 开发日志 |
| **验收前端** | — | `lab/frontend/README.md`, `server.js`, `demos.js`（**不必读** `src/observer`） |

---

## 6. 测试资产（验证行为时读这些，别扫全 test/）

| 路径 | 用途 |
|------|------|
| [`lab/test/manifest.json`](../test/manifest.json) | 全量回归清单（135 项） |
| [`lab/test/latest.md`](../test/latest.md) | 最近一次全量结果 |
| [`lab/test/run_all.py`](../test/run_all.py) | 回归入口 |
| [`lab/test/cases/sql/`](../test/cases/sql/) | 18 个自定义 SQL（一题一文件） |
| [`lab/test/comprehensive/cases.json`](../test/comprehensive/cases.json) | 100 条结果集断言 |
| [`lab/test/stress/`](../test/stress/) | big-query / big-write 脚本 |
| `test/case/test/primary-*.test` | MiniOB 官方 primary 用例（14 个） |

**协作约定（省 token）**：AI 助手默认**只写代码 + 更新 `lab/log/xc/` 日志**；编译与 `run_all.py` 由同伴执行（见 `AGENTS.md` §1.3）。

---

## 7. 文档与日志（按需）

| 路径 | 何时读 |
|------|--------|
| [`lab/doc/数据库课程设计实验报告-完整版.md`](./数据库课程设计实验报告-完整版.md) | 理解整体设计、答辩口径 |
| [`lab/题目/指导书.md`](../题目/指导书.md) | 单题需求与边界 |
| [`lab/log/yys-dev/`](../log/yys-dev/) | 各题实现过程、踩坑 |
| [`lab/log/xc/`](../log/xc/) | XC 工作区开发日志 |
| `docs/docs/design/miniob-sql-execution-process.md` | 官方 SQL 执行流程（可选） |
| `docs/docs/design/miniob-bplus-tree.md` | 索引/B+ 树（仅索引题） |

---

## 8. 建议 **不要** 全量阅读的路径

以下目录体量大、与本课题 18 题关联弱，**除非任务明确涉及**：

```text
src/oblsm/                          # LSM 引擎（本课题堆表为主）
src/cpplings/                       # C++ 练习题
benchmark/                          # 性能基准
unittest/                           # 单元测试（非课程 primary）
docs/docs/lectures/                 # 讲义（理论背景，非实现）
src/observer/sql/optimizer/cascade/ # Cascade 优化器框架
src/observer/storage/clog/          # 日志（本课题未深改）
src/observer/storage/trx/mvcc_*     # update-mvcc 未实现
.github/                            # CI 配置
build*/                               # 编译产物
lab/frontend/runtime/               # 运行时数据
```

---

## 9. 关键数据结构速查

| 概念 | 位置 |
|------|------|
| 语法树节点 | `parse_defs.h` → `ParsedSqlNode`, `SelectSqlNode`, `UpdateSqlNode` |
| 语句类型 | `stmt.h` → `StmtType`, `SCF_*` |
| 表达式类型 | `expression.h` → `ExprType`, `ComparisonExpr`, `FieldExpr` |
| 算子类型 | `logical_operator.h`, `physical_operator.h` |
| 记录布局 | `[trx字段][null bitmap][用户字段]`，`table_meta.cpp` |
| 错误码 | `src/observer/common/rc.h` → `RC` |

---

## 10. 常用命令

```bash
# 编译（同伴本地 / Docker）
bash build.sh debug --make -j8

# 全量回归
python3 lab/test/run_all.py

# 启动 observer
build_debug/bin/observer -f etc/observer.ini -p 6789 -P plain

# 答辩前端
node lab/frontend/server.js
```

---

## 11. 给 Codex 的推荐工作流

1. 读 **本文 §2 + §5** 定位层次与文件清单。  
2. 读 **对应用例**（§6）列出边界场景。  
3. **只打开 §5 表中列出的文件**，必要时向上/向下追踪 1 层调用。  
4. 改 Parser → 改 Binder → 改 Plan/Operator → 改 Storage（**自顶向下**，避免只改存储层）。  
5. 更新 `lab/log/xc/` 当前阶段唯一日志；报告相关改动写 `lab/doc/`。  
6. 不主动跑编译/全量测试（除非用户明确要求）。

---

*最后同步：与 `lab/test/latest.md`（135/135 通过）、实验报告完整版一致。仓库变更实现范围或测试结论后，请更新 §3 与 §6。*
