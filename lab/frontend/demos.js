// 每个功能一个独立的演示 SQL 集合。
// 表名加 demo_ 前缀避免互相冲突；不同 demo 用不同表名，可并行点。
// 字段顺序和值参考 yys/test/*.sql 与 lab/test/cases/sql/*.sql，
// 已经过 35/35 全量回归测试（见 lab/log/yys-dev/20全量测试报告-bupt-lab.md）。
//
// 注意：MiniOB 不支持 `DROP TABLE IF EXISTS`（语法识别但语义上对不存在的表
// 返回 FAILURE）。本 demo 不写 DROP IF EXISTS；如果要重跑某个 demo，请先点
// 顶部的「🔄 重置 demo 数据」按钮把 demo_* 表清掉。
//
// 已知 demo 表清单（用于「重置」按钮）：
const DEMO_TABLES = [
  "demo_basic", "demo_drop", "demo_update", "demo_date",
  "demo_ag", "demo_like", "demo_j1", "demo_j2", "demo_j3",
  "demo_fn", "demo_ob", "demo_gb",
  "demo_mi", "demo_uq", "demo_null",
  "demo_ssq_1", "demo_ssq_2", "demo_csq_1", "demo_csq_2", "demo_csq_3",
  "demo_us1", "demo_us2",
  "demo_bq_a", "demo_bq_b", "demo_bw",
];

const DEMOS = [
  {
    id: "basic",
    title: "basic — 建表/插入/查询/DESC",
    group: "DDL & 基础查询",
    sql: [
      "CREATE TABLE demo_basic(id INT, name CHAR(10));",
      "INSERT INTO demo_basic VALUES (1, 'ok');",
      "SELECT * FROM demo_basic;",
      "DESC demo_basic;",
    ],
  },
  {
    id: "drop_table",
    title: "drop-table — 删除表",
    group: "DDL & 基础查询",
    sql: [
      "CREATE TABLE demo_drop(id INT, name CHAR(10));",
      "INSERT INTO demo_drop VALUES (1, 'x');",
      "SELECT * FROM demo_drop;",
      "DROP TABLE demo_drop;",
      {
        sql: "DROP TABLE demo_drop_not_exist;",
        expectedFailure: true,
        note: "预期失败：用于演示 MiniOB 对不存在表返回 FAILURE，而不是 DROP TABLE IF EXISTS 语义。",
      },
    ],
  },
  {
    id: "update",
    title: "update — 单行/全表更新 + 索引同步",
    group: "DML",
    sql: [
      "CREATE TABLE demo_update(id INT, name CHAR(10), col1 INT);",
      "CREATE INDEX i_demo_update_id ON demo_update(id);",
      "INSERT INTO demo_update VALUES (1, 'N1', 1);",
      "INSERT INTO demo_update VALUES (2, 'N2', 1);",
      "UPDATE demo_update SET name='N01' WHERE id=1;",
      "SELECT * FROM demo_update;",
      "UPDATE demo_update SET col1=0;",
      "SELECT * FROM demo_update;",
    ],
  },
  {
    id: "date",
    title: "date — 日期类型 + 范围索引",
    group: "类型 & 函数",
    sql: [
      "CREATE TABLE demo_date(id int, u_date date);",
      "CREATE INDEX i_demo_date ON demo_date(u_date);",
      "INSERT INTO demo_date VALUES (1, '2020-01-21');",
      "INSERT INTO demo_date VALUES (2, '2020-10-21');",
      "INSERT INTO demo_date VALUES (3, '2020-1-01');",
      "INSERT INTO demo_date VALUES (4, '2016-2-29');",
      "SELECT * FROM demo_date WHERE u_date>'2020-1-20';",
      "SELECT * FROM demo_date WHERE u_date='2020-1-1';",
    ],
  },
  {
    id: "aggregation",
    title: "aggregation — count/min/max/avg",
    group: "聚合",
    sql: [
      "CREATE TABLE demo_ag(id int, num int, price float, addr char(10));",
      "INSERT INTO demo_ag VALUES (1, 18, 10.0, 'abc');",
      "INSERT INTO demo_ag VALUES (2, 15, 20.0, 'abc');",
      "INSERT INTO demo_ag VALUES (3, 12, 30.0, 'def');",
      "INSERT INTO demo_ag VALUES (4, 15, 30.0, 'dei');",
      "SELECT count(*) FROM demo_ag;",
      "SELECT count(num) FROM demo_ag;",
      "SELECT min(num), max(num), avg(num) FROM demo_ag;",
    ],
  },
  {
    id: "like",
    title: "like — 模式匹配 % _",
    group: "WHERE 扩展",
    sql: [
      "CREATE TABLE demo_like(id int, name char(10));",
      "INSERT INTO demo_like VALUES (1, 'abc');",
      "INSERT INTO demo_like VALUES (2, 'abcd');",
      "INSERT INTO demo_like VALUES (3, 'def');",
      "INSERT INTO demo_like VALUES (4, 'x');",
      "SELECT * FROM demo_like WHERE name LIKE 'abc';",
      "SELECT * FROM demo_like WHERE name LIKE 'abc%';",
      "SELECT * FROM demo_like WHERE name LIKE '%bc_';",
      "SELECT * FROM demo_like WHERE name LIKE '_bc';",
    ],
  },
  {
    id: "join",
    title: "join — 两/三表 INNER JOIN",
    group: "多表",
    sql: [
      "CREATE TABLE demo_j1(id int, name char);",
      "CREATE TABLE demo_j2(id int, num int);",
      "CREATE TABLE demo_j3(id int, num2 int);",
      "INSERT INTO demo_j1 VALUES (1, 'a');",
      "INSERT INTO demo_j1 VALUES (2, 'b');",
      "INSERT INTO demo_j1 VALUES (3, 'c');",
      "INSERT INTO demo_j2 VALUES (1, 2);",
      "INSERT INTO demo_j2 VALUES (2, 15);",
      "INSERT INTO demo_j3 VALUES (1, 120);",
      "INSERT INTO demo_j3 VALUES (3, 800);",
      "SELECT * FROM demo_j1 INNER JOIN demo_j2 ON demo_j1.id=demo_j2.id;",
      "SELECT * FROM demo_j1 INNER JOIN demo_j2 ON demo_j1.id=demo_j2.id INNER JOIN demo_j3 ON demo_j1.id=demo_j3.id;",
    ],
  },
  {
    id: "function",
    title: "function — LENGTH / ROUND / DATE_FORMAT",
    group: "类型 & 函数",
    sql: [
      "CREATE TABLE demo_fn(id int, name char, score float, u_date date);",
      "INSERT INTO demo_fn VALUES (1, 'abc', 1.5, '2020-01-21');",
      "INSERT INTO demo_fn VALUES (2, 'hello', 2.6, '2016-2-29');",
      "SELECT LENGTH(name) FROM demo_fn;",
      "SELECT ROUND(score) FROM demo_fn;",
      "SELECT DATE_FORMAT(u_date, '%Y-%m-%d') FROM demo_fn;",
    ],
  },
  {
    id: "order_by",
    title: "order-by — 多键 ASC/DESC",
    group: "排序",
    sql: [
      "CREATE TABLE demo_ob(id int, score float, name char);",
      "INSERT INTO demo_ob VALUES (3, 1.0, 'a');",
      "INSERT INTO demo_ob VALUES (1, 2.0, 'b');",
      "INSERT INTO demo_ob VALUES (4, 3.0, 'c');",
      "INSERT INTO demo_ob VALUES (3, 2.0, 'c');",
      "INSERT INTO demo_ob VALUES (3, 4.0, 'c');",
      "SELECT * FROM demo_ob ORDER BY id;",
      "SELECT * FROM demo_ob ORDER BY score DESC;",
      "SELECT * FROM demo_ob ORDER BY id DESC, score ASC, name DESC;",
    ],
  },
  {
    id: "group_by",
    title: "group-by — 多字段 / HAVING",
    group: "聚合",
    sql: [
      "CREATE TABLE demo_gb(id int, score float, name char);",
      "INSERT INTO demo_gb VALUES (3, 1.0, 'a');",
      "INSERT INTO demo_gb VALUES (1, 2.0, 'b');",
      "INSERT INTO demo_gb VALUES (4, 3.0, 'c');",
      "INSERT INTO demo_gb VALUES (3, 2.0, 'c');",
      "INSERT INTO demo_gb VALUES (3, 4.0, 'c');",
      "INSERT INTO demo_gb VALUES (3, 3.0, 'd');",
      "SELECT id, avg(score) FROM demo_gb GROUP BY id;",
      "SELECT name, count(id) FROM demo_gb GROUP BY name HAVING count(id) > 1;",
      "SELECT id, avg(score) FROM demo_gb GROUP BY id HAVING avg(score) > 2;",
    ],
  },
  {
    id: "multi_index",
    title: "multi-index — 复合索引 + DML 同步",
    group: "索引",
    sql: [
      "CREATE TABLE demo_mi(id int, col1 int, col2 float, col3 char, col4 date, col5 int, col6 int);",
      "CREATE INDEX i_mi_12 ON demo_mi(col1, col2);",
      "CREATE INDEX i_mi_56 ON demo_mi(col5, col6);",
      "INSERT INTO demo_mi VALUES (1, 1, 11.2, 'a', '2021-01-02', 1, 1);",
      "INSERT INTO demo_mi VALUES (2, 1, 16.2, 'x', '2021-01-02', 1, 61);",
      "INSERT INTO demo_mi VALUES (3, 1, 11.6, 'h', '2023-01-02', 10, 17);",
      "INSERT INTO demo_mi VALUES (4, 2, 12.2, 'e', '2022-01-04', 13, 10);",
      "SELECT * FROM demo_mi WHERE col1 = 1;",
      "SELECT * FROM demo_mi WHERE col1 = 1 AND col2 = 11.2;",
      "DELETE FROM demo_mi WHERE id = 1;",
      "SELECT * FROM demo_mi;",
    ],
  },
  {
    id: "unique",
    title: "unique — UNIQUE 索引约束",
    group: "索引",
    sql: [
      "CREATE TABLE demo_uq(id int, col1 int, col2 int);",
      "CREATE UNIQUE INDEX i_uq_id ON demo_uq(id);",
      "INSERT INTO demo_uq VALUES (1, 1, 1);",
      "INSERT INTO demo_uq VALUES (2, 1, 1);",
      {
        sql: "INSERT INTO demo_uq VALUES (1, 2, 1);",
        expectedFailure: true,
        note: "预期失败：用于演示 UNIQUE 索引生效，重复 id 插入会被拒绝。",
      },
      "SELECT * FROM demo_uq;",
    ],
  },
  {
    id: "null",
    title: "null — NULL/NOT NULL + IS NULL/IS NOT NULL",
    group: "NULL",
    sql: [
      "CREATE TABLE demo_null(id int not null, num int null, price float not null, birthday date null);",
      "INSERT INTO demo_null VALUES (1, 18, 10.0, '2020-01-01');",
      "INSERT INTO demo_null VALUES (2, null, 20.0, '2010-01-11');",
      "INSERT INTO demo_null VALUES (3, 12, 30.0, null);",
      "INSERT INTO demo_null VALUES (4, 15, 30.0, '2021-01-31');",
      "SELECT * FROM demo_null WHERE num IS NULL;",
      "SELECT * FROM demo_null WHERE birthday IS NOT NULL;",
      "SELECT count(num) FROM demo_null;",
      "SELECT avg(num) FROM demo_null;",
    ],
  },
  {
    id: "simple_sub_query",
    title: "simple-sub-query — IN / 标量比较",
    group: "子查询",
    sql: [
      "CREATE TABLE demo_ssq_1(id int, col1 int, feat1 float);",
      "CREATE TABLE demo_ssq_2(id int, col2 int, feat2 float);",
      "INSERT INTO demo_ssq_1 VALUES (1, 4, 11.2);",
      "INSERT INTO demo_ssq_1 VALUES (2, 2, 12.0);",
      "INSERT INTO demo_ssq_2 VALUES (1, 2, 13.0);",
      "INSERT INTO demo_ssq_2 VALUES (2, 7, 10.5);",
      "INSERT INTO demo_ssq_2 VALUES (5, 3, 12.6);",
      "SELECT * FROM demo_ssq_1 WHERE id IN (SELECT id FROM demo_ssq_2);",
      "SELECT * FROM demo_ssq_1 WHERE col1 = (SELECT avg(col2) FROM demo_ssq_2);",
      "SELECT * FROM demo_ssq_1 WHERE feat1 >= (SELECT min(feat2) FROM demo_ssq_2);",
    ],
  },
  {
    id: "complex_sub_query",
    title: "complex-sub-query — 嵌套 + 关联子查询",
    group: "子查询",
    sql: [
      "CREATE TABLE demo_csq_1(id int, col1 int, feat1 float);",
      "CREATE TABLE demo_csq_2(id int, col2 int, feat2 float);",
      "CREATE TABLE demo_csq_3(id int, col3 int, feat3 float);",
      "INSERT INTO demo_csq_1 VALUES (1, 4, 11.2);",
      "INSERT INTO demo_csq_1 VALUES (2, 2, 12.0);",
      "INSERT INTO demo_csq_1 VALUES (3, 3, 13.5);",
      "INSERT INTO demo_csq_2 VALUES (1, 2, 13.0);",
      "INSERT INTO demo_csq_2 VALUES (2, 7, 10.5);",
      "INSERT INTO demo_csq_3 VALUES (1, 2, 11.0);",
      "INSERT INTO demo_csq_3 VALUES (3, 6, 16.5);",
      "SELECT * FROM demo_csq_1 WHERE id IN (SELECT id FROM demo_csq_2 WHERE id IN (SELECT id FROM demo_csq_3));",
      "SELECT * FROM demo_csq_1 WHERE feat1 <> (SELECT avg(feat2) FROM demo_csq_2 WHERE feat2 > demo_csq_1.feat1);",
    ],
  },
  {
    id: "update_select",
    title: "update-select — SET 右值=子查询",
    group: "DML",
    sql: [
      "CREATE TABLE demo_us1(id INT, val INT);",
      "CREATE TABLE demo_us2(id INT, score INT);",
      "INSERT INTO demo_us1 VALUES (1, 10);",
      "INSERT INTO demo_us1 VALUES (2, 20);",
      "INSERT INTO demo_us2 VALUES (1, 100);",
      "INSERT INTO demo_us2 VALUES (2, 200);",
      "-- 标量子查询：把 id=1 的 score 赋给全部行",
      "UPDATE demo_us1 SET val = (SELECT score FROM demo_us2 WHERE demo_us2.id = 1);",
      "SELECT * FROM demo_us1;",
      "-- 关联子查询：每个外层行匹配对应的 score",
      "UPDATE demo_us1 SET val = (SELECT score FROM demo_us2 WHERE demo_us2.id = demo_us1.id);",
      "SELECT * FROM demo_us1;",
      "-- 多行标量子查询应失败（无 WHERE 限定）：当前实现预期行为",
      {
        sql: "UPDATE demo_us1 SET val = (SELECT score FROM demo_us2);",
        expectedFailure: true,
        note: "预期失败：多行结果不能直接作为标量子查询赋值给 SET 右值。",
      },
    ],
  },
  {
    id: "big_query",
    title: "big-query — 800 行压力 (count/JOIN/GROUP)",
    group: "压力",
    sql: null,
    generator: "big_query",
  },
  {
    id: "big_write",
    title: "big-write — 500 ops 压力 (insert/update/delete)",
    group: "压力",
    sql: null,
    generator: "big_write",
  },
];

// 生成器：每次 demo 需要新的脚本内容时调用，返回一组 SQL 字符串。
// 复用 lab/test/stress/gen_*.py 的逻辑，纯 JS 重新实现，输出与 stress 脚本一致。
function generateBigQuery() {
  const lines = [];
  lines.push("CREATE TABLE demo_bq_a(id INT, g INT, v INT);");
  lines.push("CREATE TABLE demo_bq_b(id INT, h INT);");
  // 800 行：a 1000~1799，b 1000~1799（id 重合）
  for (let i = 0; i < 800; i++) {
    const id = 1000 + i;
    const g = i % 10;
    const v = (i * 7) % 100;
    lines.push(`INSERT INTO demo_bq_a VALUES (${id}, ${g}, ${v});`);
    if (i < 800) {
      lines.push(`INSERT INTO demo_bq_b VALUES (${id}, ${id % 5});`);
    }
  }
  lines.push("SELECT count(*) FROM demo_bq_a;");
  lines.push("SELECT count(*) FROM demo_bq_b;");
  lines.push("SELECT g, count(*), avg(v) FROM demo_bq_a GROUP BY g HAVING count(*) > 50 ORDER BY g;");
  lines.push("SELECT count(*) FROM demo_bq_a, demo_bq_b WHERE demo_bq_a.id = demo_bq_b.id AND demo_bq_b.h = 2 AND demo_bq_a.g < 5;");
  return lines;
}

function generateBigWrite() {
  const lines = [];
  lines.push("CREATE TABLE demo_bw(id INT, v INT);");
  // 200 行插入
  for (let i = 1; i <= 200; i++) {
    lines.push(`INSERT INTO demo_bw VALUES (${i}, ${i * 2});`);
  }
  // 200 行 update（v = v + 1）
  for (let i = 1; i <= 200; i++) {
    lines.push(`UPDATE demo_bw SET v = v + 1 WHERE id = ${i};`);
  }
  // 100 行 delete（id <= 100）
  for (let i = 1; i <= 100; i++) {
    lines.push(`DELETE FROM demo_bw WHERE id = ${i};`);
  }
  lines.push("SELECT count(*) FROM demo_bw;");
  lines.push("SELECT min(id), max(id), sum(v) FROM demo_bw;");
  return lines;
}

function resolveDemo(entry) {
  if (entry.sql) return entry.sql;
  if (entry.generator === "big_query") return generateBigQuery();
  if (entry.generator === "big_write") return generateBigWrite();
  return [];
}
