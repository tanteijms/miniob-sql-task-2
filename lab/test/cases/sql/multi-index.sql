CREATE TABLE multi_index(id int, col1 int, col2 float, col3 char, col4 date, col5 int, col6 int);
CREATE INDEX i_1_12 ON multi_index(col1, col2);
CREATE INDEX i_1_345 ON multi_index(col3, col4, col5);
CREATE INDEX i_1_56 ON multi_index(col5, col6);
CREATE INDEX i_1_456 ON multi_index(col4, col5, col6);

CREATE TABLE multi_index2(id int, col1 int, col2 float, col3 char, col4 date, col5 int, col6 int);
INSERT INTO multi_index2 VALUES (1, 1, 11.2, 'a', '2021-01-02', 1, 1);
INSERT INTO multi_index2 VALUES (2, 1, 16.2, 'x', '2021-01-02', 1, 61);
INSERT INTO multi_index2 VALUES (3, 1, 11.6, 'h', '2023-01-02', 10, 17);
CREATE INDEX i_2_12 ON multi_index2(col1, col2);
CREATE INDEX i_2_345 ON multi_index2(col3, col4, col5);
CREATE INDEX i_2_56 ON multi_index2(col5, col6);
CREATE INDEX i_2_456 ON multi_index2(col4, col5, col6);

CREATE TABLE multi_index3(id int, col1 int, col2 float, col3 char, col4 date, col5 int, col6 int);
CREATE INDEX i_3_i1 ON multi_index3(id, col1);
INSERT INTO multi_index3 VALUES (1, 1, 11.2, 'a', '2021-01-02', 1, 1);
INSERT INTO multi_index3 VALUES (2, 1, 16.2, 'x', '2021-01-02', 1, 61);
CREATE INDEX i_3_14 ON multi_index3(col1, col4);
INSERT INTO multi_index3 VALUES (3, 1, 11.6, 'h', '2023-01-02', 10, 17);
INSERT INTO multi_index3 VALUES (4, 2, 12.2, 'e', '2022-01-04', 13, 10);
INSERT INTO multi_index3 VALUES (5, 3, 14.2, 'd', '2020-04-02', 12, 2);

SELECT * FROM multi_index3 WHERE id = 1;
SELECT * FROM multi_index3 WHERE col1 > 1 AND col4 = '2021-01-02';

DELETE FROM multi_index3 WHERE id = 1;
DELETE FROM multi_index3 WHERE col3 = 'x';
UPDATE multi_index3 SET col6=49 WHERE id=2;
UPDATE multi_index3 SET col1=2 WHERE id=2;

CREATE TABLE multi_index4(id int, col1 int, col2 float, col3 char, col4 date, col5 int, col6 int);
CREATE INDEX i_4_i7 ON multi_index4(id, col7);
