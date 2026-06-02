# MiniOB 课程设计开发日志 — multi-index

> 仓库：`miniob-sql-task-2`  
> 分支：`dev-yys`  
> 个人工作区：`yys/`（不提交）  
> 记录时间：2026-06-02  
> 前置：[09](./09开发日志-group-by.md)

---

## 1. 任务说明

- 支持 `CREATE INDEX idx ON t(c1, c2, ...)` 复合索引
- 空表 / 非空表建索引、INSERT / DELETE / UPDATE 后索引一致
- 复合键按字段顺序字典序比较
- 官方用例：`test/case/test/primary-multi-index.test`

---

## 2. 测试结果：**通过**

| 场景 | 结果 |
|------|------|
| 空表建 2～3 列复合索引 | SUCCESS |
| 有数据后建复合索引并回填 | SUCCESS |
| 先建索引再 INSERT / DELETE / UPDATE | SUCCESS |
| 不存在的列 `col7` 建索引 | FAILURE（符合预期） |
| `primary-multi-index.test` DDL/DML | 全部 SUCCESS（SELECT 行为 `-- sort` 注释） |

---

## 3. 改前状态

| 层次 | 状态 |
|------|------|
| 语法 `CREATE INDEX ... (ID)` 单列 | ✅ |
| `IndexMeta` / B+Tree 键 | ❌ 仅单字段 |
| DML 索引维护 | ✅ 有框架，需适配复合键 |

---

## 4. 实现说明

### 4.1 复合键编码

按索引字段顺序，从 record 中截取各列定长字节拼接：

```text
key = col1_bytes || col2_bytes || ... || colN_bytes
B+Tree 键 = key || RID
```

### 4.2 比较器

`AttrComparator` 扩展为支持多列：逐字段 `DataType::compare`，不等则返回，相等则比较下一列。

`IndexFileHeader` 增加 `field_num`、`field_types[]`、`field_lengths[]`（最多 16 列），旧单列索引 `field_num=0` 仍走原逻辑。

### 4.3 改动文件

| 文件 | 作用 |
|------|------|
| `yacc_sql.y` / `parse_defs.h` | `CREATE INDEX ... (attr_list)` |
| `index_meta.h/cpp` | 多字段元数据 + JSON `field_names` |
| `create_index_stmt.*` | 校验多列并绑定 `FieldMeta` |
| `index.h/cpp` | `make_key()` 复合键拼接 |
| `bplus_tree.h/cpp` | 复合比较器 + 文件头扩展 |
| `bplus_tree_index.*` | create/open/insert/delete 用复合键 |
| `heap_table_engine.cpp` | 建索引 / open 多列；DML 路径复用原有 `insert/delete_entry_of_indexes` |
| `table*.h/cpp` | `create_index` 签名改为 `vector<const FieldMeta *>` |

### 4.4 数据流

```text
CREATE INDEX i ON t(c1,c2)
  → yacc: attribute_names=[c1,c2]
  → IndexMeta.init(name, field_metas)
  → BplusTreeIndex.create(types, lengths)
  → 扫描已有 record，make_key → insert_entry
  → TableMeta 持久化 field_names
```

---

## 5. 建议提交

```bash
git add \
  src/observer/sql/parser/parse_defs.h \
  src/observer/sql/parser/yacc_sql.y \
  src/observer/sql/stmt/create_index_stmt.h \
  src/observer/sql/stmt/create_index_stmt.cpp \
  src/observer/sql/executor/create_index_executor.cpp \
  src/observer/storage/index/index_meta.h \
  src/observer/storage/index/index_meta.cpp \
  src/observer/storage/index/index.h \
  src/observer/storage/index/index.cpp \
  src/observer/storage/index/bplus_tree.h \
  src/observer/storage/index/bplus_tree.cpp \
  src/observer/storage/index/bplus_tree_index.h \
  src/observer/storage/index/bplus_tree_index.cpp \
  src/observer/storage/table/table.h \
  src/observer/storage/table/table.cpp \
  src/observer/storage/table/table_engine.h \
  src/observer/storage/table/heap_table_engine.h \
  src/observer/storage/table/heap_table_engine.cpp \
  src/observer/storage/table/lsm_table_engine.h

git commit -m "$(cat <<'EOF'
feat: support multi-column composite B+ tree indexes

Extend parser and IndexMeta for multi-field keys, encode composite
keys in BplusTreeIndex, and compare columns lexicographically in order.
EOF
)"
```

---

## 6. 进度索引（lab/log）

| 序号 | 题目 | 日志 |
|------|------|------|
| 01–09 | … | 见前序日志 |
| **10** | **multi-index** | **本文** |

---

## 7. 后续建议

| 题目 | 分值 |
|------|------|
| unique | 与 multi-index 共享键比较逻辑 |
| null | 3 |
| simple-sub-query | 4 |
