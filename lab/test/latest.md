# MiniOB 全量测试报告

- 开始时间：2026-06-03T16:33:59+08:00
- 结束时间：2026-06-03T16:35:50+08:00
- 分支：`1357`  commit `bf31e7c6`
- observer：`/Users/yishuoyan/projects/bupt/25-26-2/miniob-sql-task-2/build_debug/bin/observer`

## 汇总

| 指标 | 数值 |
|------|------|
| 用例总数 | 135 |
| 通过 | 135 |
| 失败 | 0 |
| 跳过 | 0 |
| 通过率 | 100.0% |
| 总耗时 | 111.5s |

## 官方用例

| 状态 | 用例 | 功能 | 分值 | 耗时 | 说明 |
|------|------|------|------|------|------|
| ✅ | official-basic | basic | 2 | 0.5s | ok |
| ✅ | official-drop-table | drop-table | 2 | 0.7s | ok |
| ✅ | official-update | update | 2 | 0.4s | ok |
| ✅ | official-date | date | 2 | 0.4s | ok |
| ✅ | official-aggregation | aggregation-func | 2 | 0.4s | ok |
| ✅ | official-join | join-tables | 2 | 13.8s | ok |
| ✅ | official-order-by | order-by | 2 | 0.5s | ok |
| ✅ | official-group-by | group-by | 4 | 0.4s | ok |
| ✅ | official-multi-index | multi-index | 4 | 0.9s | ok |
| ✅ | official-expression | function | 2 | 0.4s | ok |
| ✅ | official-unique | unique | 2 | 0.4s | ok |
| ✅ | official-simple-sub-query | simple-sub-query | 4 | 0.5s | ok |
| ✅ | official-complex-sub-query | complex-sub-query | 5 | 0.5s | ok |
| ✅ | official-null | null | 3 | 0.5s | ok |

## 自定义 SQL

| 状态 | 用例 | 功能 | 分值 | 耗时 | 说明 |
|------|------|------|------|------|------|
| ✅ | custom-basic | basic | 2 | 0.4s | ok |
| ✅ | custom-drop-table | drop-table | 2 | 0.4s | ok |
| ✅ | custom-update | update | 2 | 0.4s | ok |
| ✅ | custom-update-index | update | 2 | 0.4s | ok |
| ✅ | custom-date | date | 2 | 0.4s | ok |
| ✅ | custom-date-delete | date | 2 | 0.4s | ok |
| ✅ | custom-aggregation | aggregation-func | 2 | 0.4s | ok |
| ✅ | custom-like | like | 2 | 0.4s | ok |
| ✅ | custom-join | join-tables | 2 | 0.5s | ok |
| ✅ | custom-function | function | 2 | 0.4s | ok |
| ✅ | custom-order-by | order-by | 2 | 0.4s | ok |
| ✅ | custom-group-by | group-by | 4 | 0.4s | ok |
| ✅ | custom-multi-index | multi-index | 4 | 1.0s | ok |
| ✅ | custom-unique | unique | 2 | 0.4s | ok |
| ✅ | custom-simple-sub-query | simple-sub-query | 4 | 0.4s | ok |
| ✅ | custom-complex-sub-query | complex-sub-query | 5 | 0.5s | ok |
| ✅ | custom-null | null | 3 | 0.4s | ok |
| ✅ | custom-update-select | update-select | 4 | 0.5s | ok |

## 综合断言用例 (100)

| 状态 | 用例 | 功能 | 分值 | 耗时 | 说明 |
|------|------|------|------|------|------|
| ✅ | comp-001 | basic | 0 | 0.7s | ok |
| ✅ | comp-002 | basic | 0 | 0.8s | ok |
| ✅ | comp-003 | basic | 0 | 0.8s | ok |
| ✅ | comp-004 | basic | 0 | 0.7s | ok |
| ✅ | comp-006 | drop-table | 0 | 0.8s | ok |
| ✅ | comp-008 | drop-table | 0 | 0.8s | ok |
| ✅ | comp-009 | update | 0 | 0.8s | ok |
| ✅ | comp-010 | update | 0 | 0.8s | ok |
| ✅ | comp-011 | update | 0 | 0.8s | ok |
| ✅ | comp-012 | update | 0 | 0.8s | ok |
| ✅ | comp-014 | update | 0 | 0.8s | ok |
| ✅ | comp-015 | update | 0 | 0.8s | ok |
| ✅ | comp-016 | date | 0 | 0.8s | ok |
| ✅ | comp-017 | date | 0 | 0.4s | ok |
| ✅ | comp-018 | date | 0 | 0.8s | ok |
| ✅ | comp-019 | date | 0 | 0.8s | ok |
| ✅ | comp-020 | date | 0 | 0.8s | ok |
| ✅ | comp-021 | date | 0 | 0.8s | ok |
| ✅ | comp-022 | date | 0 | 0.8s | ok |
| ✅ | comp-023 | aggregation | 0 | 0.8s | ok |
| ✅ | comp-024 | aggregation | 0 | 0.8s | ok |
| ✅ | comp-025 | aggregation | 0 | 0.8s | ok |
| ✅ | comp-026 | aggregation | 0 | 0.8s | ok |
| ✅ | comp-031 | like | 0 | 0.8s | ok |
| ✅ | comp-032 | like | 0 | 0.8s | ok |
| ✅ | comp-033 | like | 0 | 0.8s | ok |
| ✅ | comp-035 | like | 0 | 0.8s | ok |
| ✅ | comp-036 | join | 0 | 0.8s | ok |
| ✅ | comp-037 | join | 0 | 0.8s | ok |
| ✅ | comp-038 | join | 0 | 0.8s | ok |
| ✅ | comp-039 | join | 0 | 0.9s | ok |
| ✅ | comp-040 | join | 0 | 0.8s | ok |
| ✅ | comp-042 | join | 0 | 0.8s | ok |
| ✅ | comp-043 | join | 0 | 0.8s | ok |
| ✅ | comp-045 | join | 0 | 0.8s | ok |
| ✅ | comp-046 | function | 0 | 0.8s | ok |
| ✅ | comp-047 | function | 0 | 0.8s | ok |
| ✅ | comp-048 | function | 0 | 0.8s | ok |
| ✅ | comp-050 | function | 0 | 0.8s | ok |
| ✅ | comp-051 | function | 0 | 0.8s | ok |
| ✅ | comp-052 | function | 0 | 0.4s | ok |
| ✅ | comp-053 | order-by | 0 | 0.8s | ok |
| ✅ | comp-054 | order-by | 0 | 0.8s | ok |
| ✅ | comp-055 | order-by | 0 | 0.8s | ok |
| ✅ | comp-057 | order-by | 0 | 0.8s | ok |
| ✅ | comp-058 | order-by | 0 | 0.8s | ok |
| ✅ | comp-059 | group-by | 0 | 0.8s | ok |
| ✅ | comp-060 | group-by | 0 | 0.8s | ok |
| ✅ | comp-061 | group-by | 0 | 0.8s | ok |
| ✅ | comp-063 | group-by | 0 | 0.8s | ok |
| ✅ | comp-064 | group-by | 0 | 0.8s | ok |
| ✅ | comp-065 | group-by | 0 | 0.8s | ok |
| ✅ | comp-066 | group-by | 0 | 0.8s | ok |
| ✅ | comp-067 | group-by | 0 | 0.8s | ok |
| ✅ | comp-068 | multi-index | 0 | 0.8s | ok |
| ✅ | comp-069 | multi-index | 0 | 0.8s | ok |
| ✅ | comp-070 | multi-index | 0 | 0.8s | ok |
| ✅ | comp-071 | multi-index | 0 | 0.8s | ok |
| ✅ | comp-072 | multi-index | 0 | 0.8s | ok |
| ✅ | comp-073 | multi-index | 0 | 0.9s | ok |
| ✅ | comp-074 | multi-index | 0 | 0.9s | ok |
| ✅ | comp-075 | unique | 0 | 0.8s | ok |
| ✅ | comp-076 | unique | 0 | 0.4s | ok |
| ✅ | comp-077 | unique | 0 | 0.4s | ok |
| ✅ | comp-078 | unique | 0 | 0.8s | ok |
| ✅ | comp-079 | unique | 0 | 0.8s | ok |
| ✅ | comp-080 | unique | 0 | 0.9s | ok |
| ✅ | comp-081 | simple-sub-query | 0 | 0.9s | ok |
| ✅ | comp-082 | simple-sub-query | 0 | 0.8s | ok |
| ✅ | comp-083 | simple-sub-query | 0 | 0.8s | ok |
| ✅ | comp-084 | simple-sub-query | 0 | 0.8s | ok |
| ✅ | comp-085 | simple-sub-query | 0 | 0.8s | ok |
| ✅ | comp-086 | simple-sub-query | 0 | 0.4s | ok |
| ✅ | comp-087 | simple-sub-query | 0 | 0.4s | ok |
| ✅ | comp-088 | simple-sub-query | 0 | 0.8s | ok |
| ✅ | comp-089 | complex-sub-query | 0 | 0.8s | ok |
| ✅ | comp-090 | complex-sub-query | 0 | 0.8s | ok |
| ✅ | comp-091 | complex-sub-query | 0 | 0.8s | ok |
| ✅ | comp-092 | complex-sub-query | 0 | 0.8s | ok |
| ✅ | comp-093 | complex-sub-query | 0 | 0.8s | ok |
| ✅ | comp-094 | complex-sub-query | 0 | 0.9s | ok |
| ✅ | comp-095 | complex-sub-query | 0 | 0.8s | ok |
| ✅ | comp-096 | complex-sub-query | 0 | 0.8s | ok |
| ✅ | comp-097 | update-select | 0 | 0.8s | ok |
| ✅ | comp-098 | update-select | 0 | 0.8s | ok |
| ✅ | comp-099 | update-select | 0 | 0.8s | ok |
| ✅ | comp-100a | update-select | 0 | 0.8s | ok |
| ✅ | comp-100b | update-select | 0 | 0.4s | ok |
| ✅ | comp-100c | update-select | 0 | 0.8s | ok |
| ✅ | comp-100d | update-select | 0 | 0.8s | ok |
| ✅ | comp-101 | null | 0 | 0.8s | ok |
| ✅ | comp-102 | null | 0 | 0.8s | ok |
| ✅ | comp-103 | null | 0 | 0.8s | ok |
| ✅ | comp-104 | null | 0 | 0.4s | ok |
| ✅ | comp-105 | null | 0 | 0.4s | ok |
| ✅ | comp-106 | null | 0 | 0.8s | ok |
| ✅ | comp-107 | integration | 0 | 0.8s | ok |
| ✅ | comp-108 | integration | 0 | 0.8s | ok |
| ✅ | comp-109 | integration | 0 | 0.8s | ok |
| ✅ | comp-110 | integration | 0 | 0.8s | ok |

## 压力测试

| 状态 | 用例 | 功能 | 分值 | 耗时 | 说明 |
|------|------|------|------|------|------|
| ✅ | stress-big-write | big-write | 5 | 1.0s | count(*)=420 |
| ✅ | stress-big-write-2k | big-write | 5 | 3.2s | count(*)=1680 |
| ✅ | stress-big-query | big-query | 5 | 1.3s | count(*)=800 |

