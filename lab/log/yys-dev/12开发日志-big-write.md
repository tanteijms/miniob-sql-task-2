# MiniOB 课程设计开发日志 — big-write

> 仓库：`miniob-sql-task-2`  
> 分支：`dev-yys`  
> 记录时间：2026-06-02  
> 前置：[10](./10开发日志-multi-index.md)（索引 DML 同步）

---

## 1. 任务说明

- **大量随机增删改查**，长时间运行不崩溃、结果不错
- 核心：**索引与堆表一致性**（update/delete 后索引键正确）
- 无官方 `primary-big-write.test`，需自建压力脚本

---

## 2. 改前状态

| 路径 | 状态 |
|------|------|
| INSERT + 索引回滚 | ✅ 已有（失败时删 record + 删 index） |
| UPDATE + 索引 | ✅ 先删旧键 → 改 record → 插新键，失败回滚 |
| DELETE | ⚠️ 用 `ASSERT` 断言删索引成功；record 删失败时**索引已删、记录仍在** |

---

## 3. 本次改动

### 3.1 `HeapTableEngine::delete_record` 加固

与 `update_record_with_trx` 对齐：

```text
1. delete_entry_of_indexes(old, error_on_not_exists=false)
2. record_handler_->delete_record
3. 若 2 失败 → insert_entry_of_indexes 回滚索引
```

- 去掉 `ASSERT`，避免异常路径直接 abort
- 避免「索引已删、记录还在」的半一致状态

### 3.2 自测脚本

| 文件 | 说明 |
|------|------|
| `yys/scripts/gen_big_write_sql.py` | 固定 seed=42，500 insert + 200 update + 80 delete |
| 生成 SQL | `python3 yys/scripts/gen_big_write_sql.py > /tmp/big-write.sql` |

---

## 4. 测试结果：**通过**

| 用例 | 结果 |
|------|------|
| 500 行 + 双索引 + 200 UPDATE + 80 DELETE | SUCCESS，`count(*)=420` |
| 2000 行随机写（seed=123） | SUCCESS，`count(*)=1700` |
| 重启后 `SELECT … WHERE id=` 索引点查 | 数据仍在 |
| 重启后再 DELETE + count | 正确递减 |

验证命令：

```bash
bash build.sh debug --make -j8
rm -rf build_debug/bin/miniob/db/sys && mkdir -p build_debug/bin/miniob/db/sys
python3 yys/scripts/gen_big_write_sql.py | build_debug/bin/observer -f etc/observer.ini -P cli
# 重启后再跑: SELECT count(*) FROM big_write;
```

---

## 5. 建议提交

```bash
git add src/observer/storage/table/heap_table_engine.cpp

git commit -m "$(cat <<'EOF'
fix: rollback index entries when heap record delete fails

Align delete path with update: remove ASSERT, delete index keys first,
and re-insert index entries if record deletion fails.
EOF
)"
```

> `yys/scripts/gen_big_write_sql.py` 在 `yys/` 个人目录，不提交。

---

## 6. 与 big-query 的关系

- **big-write**：写路径 + 索引一致性 + 持久化（本题）
- **big-query**：读路径 + 大数据量 SELECT 性能（可共用同一套 bulk 数据脚本）

---

## 7. 后续建议

| 题目 | 说明 |
|------|------|
| simple-sub-query | 4 分，查询能力 |
| null | 3 分，与聚合/比较耦合 |
| unique | 依赖 multi-index 键比较 |
