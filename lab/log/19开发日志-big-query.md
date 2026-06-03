# 开发日志：big-query（5 分）

> 记录时间：2026-06-03  
> 分支：`dev-yys`  
> 前置：top8 其余题目已完成（含 update-select）  
> 说明：仓库无 `primary-big-query.test`，以压力脚本验证读路径正确性

## 策略

- 不新增 `src/` 改动（前期功能已在 big-write / join / sub-query 等用例覆盖）
- 自建 `lab/test/stress/gen_big_query_sql.py`：3 表 × 800 行 + 索引点查 / JOIN / GROUP BY / 关联子查询

## 自测

```bash
bash build.sh debug --make -j8
python3 lab/test/run_all.py --only stress-big-query   # PASS ~4s, count(*)=800
```

| 用例 | 结果 |
|------|------|
| `stress-big-query` | PASS（800 行主表 + 维表 JOIN/GROUP BY/IN 子查询） |

> 避免 3 表等大结果集笛卡尔 JOIN（易超时）；主表 800 行 + 小维表 20 行。

## 相关文件

- 压力脚本：`lab/test/stress/gen_big_query_sql.py`
- manifest：`stress-big-query`
