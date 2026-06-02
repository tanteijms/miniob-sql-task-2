-- drop-table（实现后应全部 SUCCESS）
CREATE TABLE dt(id INT, name CHAR(10));
INSERT INTO dt VALUES (1, 'x');
DROP TABLE dt;
CREATE TABLE dt(id INT);
INSERT INTO dt VALUES (1);
SELECT * FROM dt;
DROP TABLE dt_not_exist;
