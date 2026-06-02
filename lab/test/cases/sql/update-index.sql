CREATE TABLE Update_table_1(id int, t_name char, col1 int, col2 int);
CREATE INDEX index_id on Update_table_1(id);
INSERT INTO Update_table_1 VALUES (1,'N1',1,1);
INSERT INTO Update_table_1 VALUES (2,'N2',1,1);
INSERT INTO Update_table_1 VALUES (3,'N3',2,1);
UPDATE Update_table_1 SET id=4 WHERE t_name='N3';
SELECT * FROM Update_table_1;
