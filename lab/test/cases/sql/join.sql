CREATE TABLE join_table_1(id int, name char);
CREATE TABLE join_table_2(id int, num int);
CREATE TABLE join_table_3(id int, num2 int);
CREATE TABLE join_table_empty_1(id int, num_empty_1 int);

INSERT INTO join_table_1 VALUES (1, 'a');
INSERT INTO join_table_1 VALUES (2, 'b');
INSERT INTO join_table_1 VALUES (3, 'c');
INSERT INTO join_table_2 VALUES (1, 2);
INSERT INTO join_table_2 VALUES (2, 15);
INSERT INTO join_table_3 VALUES (1, 120);
INSERT INTO join_table_3 VALUES (3, 800);

SELECT * FROM join_table_1 INNER JOIN join_table_2 ON join_table_1.id=join_table_2.id;
SELECT join_table_1.name FROM join_table_1 INNER JOIN join_table_2 ON join_table_1.id=join_table_2.id;
SELECT * FROM join_table_1 INNER JOIN join_table_2 ON join_table_1.id=join_table_2.id INNER JOIN join_table_3 ON join_table_1.id=join_table_3.id;
SELECT * FROM join_table_1 INNER JOIN join_table_2 ON join_table_1.id=join_table_2.id AND join_table_2.num>13 WHERE join_table_1.name='b';
SELECT * FROM join_table_1 INNER JOIN join_table_2 ON join_table_1.id=join_table_2.id AND join_table_2.num>13 WHERE join_table_1.name='a';
SELECT * FROM join_table_1 INNER JOIN join_table_empty_1 ON join_table_1.id=join_table_empty_1.id;
