CREATE TABLE t_order_by(id int, score float, name char);
CREATE TABLE t_order_by_2(id int, age int);

INSERT INTO t_order_by VALUES (3, 1.0, 'a');
INSERT INTO t_order_by VALUES (1, 2.0, 'b');
INSERT INTO t_order_by VALUES (4, 3.0, 'c');
INSERT INTO t_order_by VALUES (3, 2.0, 'c');
INSERT INTO t_order_by VALUES (3, 4.0, 'c');
INSERT INTO t_order_by VALUES (3, 3.0, 'd');
INSERT INTO t_order_by VALUES (3, 2.0, 'f');

INSERT INTO t_order_by_2 VALUES (1, 10);
INSERT INTO t_order_by_2 VALUES (2, 20);
INSERT INTO t_order_by_2 VALUES (3, 10);
INSERT INTO t_order_by_2 VALUES (3, 20);
INSERT INTO t_order_by_2 VALUES (3, 40);
INSERT INTO t_order_by_2 VALUES (4, 20);

SELECT * FROM t_order_by ORDER BY id;
SELECT * FROM t_order_by ORDER BY score DESC;
SELECT * FROM t_order_by ORDER BY id DESC, score ASC, name DESC;
SELECT * FROM t_order_by WHERE id=3 AND name>='a' ORDER BY score DESC, name;
SELECT * FROM t_order_by, t_order_by_2 WHERE t_order_by.id=t_order_by_2.id ORDER BY t_order_by.score DESC, t_order_by_2.age ASC, t_order_by.id ASC, t_order_by.name;
