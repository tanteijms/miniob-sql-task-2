-- update-select：UPDATE SET 右值为子查询/表达式
CREATE TABLE us_t1(id INT, val INT);
CREATE TABLE us_t2(id INT, score INT);
INSERT INTO us_t1 VALUES (1, 10);
INSERT INTO us_t1 VALUES (2, 20);
INSERT INTO us_t2 VALUES (1, 100);
INSERT INTO us_t2 VALUES (2, 200);

-- 标量子查询（常量结果）
UPDATE us_t1 SET val = (SELECT score FROM us_t2 WHERE us_t2.id = 1);
SELECT * FROM us_t1;

-- 聚合子查询
UPDATE us_t1 SET val = (SELECT avg(score) FROM us_t2) WHERE id = 2;
SELECT * FROM us_t1;

-- 关联子查询
UPDATE us_t1 SET val = (SELECT score FROM us_t2 WHERE us_t2.id = us_t1.id);
SELECT * FROM us_t1;

-- 字面量仍可用（ValueExpr 表达式路径）
UPDATE us_t1 SET val = 999 WHERE id = 1;
SELECT * FROM us_t1;

-- 多行标量子查询应失败
UPDATE us_t1 SET val = (SELECT score FROM us_t2);

-- 语句级回滚：unique 冲突时前面已更新行应恢复
CREATE TABLE us_rb(id INT, val INT);
CREATE UNIQUE INDEX i_us_rb_val ON us_rb(val);
INSERT INTO us_rb VALUES (1, 10);
INSERT INTO us_rb VALUES (2, 20);
INSERT INTO us_rb VALUES (3, 15);
UPDATE us_rb SET val = (SELECT 20);
SELECT * FROM us_rb;
