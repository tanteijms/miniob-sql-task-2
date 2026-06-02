# MiniOB 课程设计开发日志 — unique（唯一索引）

> 分支：`dev-yys`  
> 记录时间：2026-06-02  
> 前置：[10](./10开发日志-multi-index.md)

---

## 1. 任务说明

- 语法：`CREATE UNIQUE INDEX idx ON t(col)`
- 插入/建索引时重复键 → `FAILURE`
- 重复索引名 → `FAILURE`
- 官方用例：`test/case/test/primary-unique.test`

---

## 2. 改动摘要

| 模块 | 改动 |
|------|------|
| 解析 | `UNIQUE` 词法；`CREATE UNIQUE INDEX` 语法；`CreateIndexSqlNode.unique` |
| 元数据 | `IndexMeta.unique_` + JSON 持久化 |
| 执行 | `CreateIndexStmt` / `Table::create_index(..., unique)` |
| 索引 | `BplusTreeIndex::insert_entry` 对 unique 索引先 `get_entry` 查重 |

> 非 unique 索引仍用 `user_key + RID` 作为 B+ 树键，允许多行相同索引列值。

---

## 3. 测试结果：**通过**

| 用例 | 结果 |
|------|------|
| `primary-unique.test` | 5 SUCCESS / 2 FAILURE（重复建索引、重复 id 插入） |
| 最终数据 | `(1,1,1)(2,1,1)(3,2,1)` 共 3 行 |
| `lab/test/run_all.sh --quick` | 全绿（含新增 official/custom unique） |

---

## 4. 建议提交

```bash
git add \
  src/observer/sql/parser/lex_sql.l \
  src/observer/sql/parser/yacc_sql.y \
  src/observer/sql/parser/parse_defs.h \
  src/observer/sql/stmt/create_index_stmt.h \
  src/observer/sql/stmt/create_index_stmt.cpp \
  src/observer/sql/executor/create_index_executor.cpp \
  src/observer/storage/index/index_meta.h \
  src/observer/storage/index/index_meta.cpp \
  src/observer/storage/index/bplus_tree_index.cpp \
  src/observer/storage/table/table.h \
  src/observer/storage/table/table.cpp \
  src/observer/storage/table/table_engine.h \
  src/observer/storage/table/heap_table_engine.h \
  src/observer/storage/table/heap_table_engine.cpp \
  src/observer/storage/table/lsm_table_engine.h

git commit -m "$(cat <<'EOF'
feat: support CREATE UNIQUE INDEX with duplicate key rejection

Parse UNIQUE index DDL, persist unique flag in IndexMeta, and reject
inserts when the indexed key already exists.
EOF
)"
```

---

## 5. 后续

- 复合唯一索引：`CREATE UNIQUE INDEX ON t(c1,c2)`（元数据已支持多列，查重按复合键）
- 与 `null` 题结合：NULL 在 unique 中的语义（多条 NULL 是否允许）
