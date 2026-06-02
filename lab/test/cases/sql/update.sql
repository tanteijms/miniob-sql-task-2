-- update（实现后应通过；参考 primary-update.test）
CREATE TABLE ut(id INT, name CHAR(10), col1 INT);
CREATE INDEX i_ut_id ON ut(id);
INSERT INTO ut VALUES (1, 'N1', 1);
INSERT INTO ut VALUES (2, 'N2', 1);
UPDATE ut SET name='N01' WHERE id=1;
SELECT * FROM ut;
UPDATE ut SET col1=0;
SELECT * FROM ut;
