CREATE TABLE t_group_by(id int, score float, name char);
CREATE TABLE t_group_by_2(id int, age int);

INSERT INTO t_group_by VALUES (3, 1.0, 'a');
INSERT INTO t_group_by VALUES (1, 2.0, 'b');
INSERT INTO t_group_by VALUES (4, 3.0, 'c');
INSERT INTO t_group_by VALUES (3, 2.0, 'c');
INSERT INTO t_group_by VALUES (3, 4.0, 'c');
INSERT INTO t_group_by VALUES (3, 3.0, 'd');
INSERT INTO t_group_by VALUES (3, 2.0, 'f');

INSERT INTO t_group_by_2 VALUES (1, 10);
INSERT INTO t_group_by_2 VALUES (2, 20);
INSERT INTO t_group_by_2 VALUES (3, 10);
INSERT INTO t_group_by_2 VALUES (3, 20);
INSERT INTO t_group_by_2 VALUES (3, 40);
INSERT INTO t_group_by_2 VALUES (4, 20);

-- 单字段 / 多字段 GROUP BY
SELECT id, avg(score) FROM t_group_by GROUP BY id;
SELECT name, min(id), max(score) FROM t_group_by GROUP BY name;
SELECT id, name, avg(score) FROM t_group_by GROUP BY id, name;

-- WHERE + GROUP BY
SELECT id, avg(score) FROM t_group_by WHERE id>2 GROUP BY id;
SELECT name, count(id), max(score) FROM t_group_by WHERE name>'a' AND id>=0 GROUP BY name;

-- 多表 + GROUP BY
SELECT t_group_by.id, t_group_by.name, avg(t_group_by.score), avg(t_group_by_2.age)
FROM t_group_by, t_group_by_2
WHERE t_group_by.id=t_group_by_2.id
GROUP BY t_group_by.id, t_group_by.name;

-- HAVING（聚合后过滤）
SELECT id, avg(score) FROM t_group_by GROUP BY id HAVING avg(score) > 2;
SELECT name, count(id), max(score) FROM t_group_by GROUP BY name HAVING count(id) > 1;
SELECT id, avg(score) FROM t_group_by GROUP BY id HAVING id > 2;
SELECT count(*) FROM t_group_by HAVING count(*) > 5;

-- GROUP BY + HAVING + ORDER BY
SELECT id, avg(score) FROM t_group_by GROUP BY id HAVING avg(score) > 2 ORDER BY id DESC;
