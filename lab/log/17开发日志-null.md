# 开发日志：null（3 分）

> 记录时间：2026-06-03  
> 分支：`dev-yys`  
> 题目要求：见 `yys/doc/速成/MiniOB.md` §1.12

## 目标

- 列定义 `NULL` / `NOT NULL`
- 插入/更新 NULL；违反 NOT NULL → `FAILURE`
- `IS NULL` / `IS NOT NULL`
- NULL 与普通比较结果为 false（WHERE 中按 false 过滤）
- 聚合：`count(*)` vs `count(col)`；avg/min/max 跳过 NULL

## 主要改动

| 模块 | 内容 |
|------|------|
| Parser | `NULL`/`IS` token；列 nullability；值 `NULL`；`IS [NOT] NULL` 谓词 |
| Value | `is_null()` / `set_null()` |
| FieldMeta | `nullable_` + JSON 持久化 |
| TableMeta | record 内 null bitmap 布局 |
| Table | `make_record` 写 bitmap + NOT NULL 校验 |
| RowTuple | 读字段时检查 bitmap |
| Expr | `ComparisonExpr` NULL 三值语义；聚合跳过 NULL |

## 自测

```bash
bash build.sh debug --make -j8
python3 lab/test/run_all.py --only official-null,custom-null
python3 lab/test/run_all.py --quick
```

- 官方 `primary-null.test`：`expected_failure=2`（L14/L15 NOT NULL 插入失败）
- 全量 quick：**31/31 PASS**

## 已知缺口

- 官方用例 §2–4 大量 `-- sort SELECT` 未在 runner 中执行（语法/语义已实现）
- DELETE/UPDATE 旧式 `condition_list` 路径无 `IS NULL`
- `UPDATE SET col = NULL` 未单独测
