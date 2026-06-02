CREATE TABLE func_t(id int, name char, score float, u_date date);
INSERT INTO func_t VALUES (1, 'abc', 1.5, '2020-01-21');
INSERT INTO func_t VALUES (2, 'hello', 2.6, '2016-2-29');

SELECT LENGTH(name) FROM func_t;
SELECT ROUND(score) FROM func_t;
SELECT DATE_FORMAT(u_date, '%Y-%m-%d') FROM func_t;
SELECT DATE_FORMAT(u_date, '%D,%M,%Y') FROM func_t WHERE id=1;

SELECT LENGTH(score) FROM func_t;
SELECT ROUND(name) FROM func_t;
