-- NULL manual verification script
-- Important: clear old database files before running because row record layout changed.

-- 1. schema and base data
CREATE TABLE null_table(id int not null, num int null, price float not null, birthday date null);
CREATE TABLE null_table2(id int not null, num int null, price float not null, birthday date null);
CREATE INDEX index_num on null_table(num);

INSERT INTO null_table VALUES (1, 18, 10.0, '2020-01-01');
INSERT INTO null_table VALUES (2, null, 20.0, '2010-01-11');
INSERT INTO null_table VALUES (3, 12, 30.0, null);
INSERT INTO null_table VALUES (4, 15, 30.0, '2021-01-31');
INSERT INTO null_table2 VALUES (1, 18, 30.0, '2021-01-31');
INSERT INTO null_table2 VALUES (2, null, 40.0, null);

-- Expect failure: price is NOT NULL
INSERT INTO null_table VALUES (5, 15, null, '2021-01-31');

-- Expect failure: id is NOT NULL
INSERT INTO null_table VALUES (null, 15, 30.0, '2021-01-31');

-- 2. full scan and visible NULL output
SELECT * FROM null_table;

-- 3. constant predicates
-- Expect 0 rows
SELECT * FROM null_table WHERE 1 IS NULL;

-- Expect all rows
SELECT * FROM null_table WHERE 1 IS NOT NULL;

-- Expect 0 rows
SELECT * FROM null_table WHERE null = 1;
SELECT * FROM null_table WHERE 1 = null;
SELECT * FROM null_table WHERE 1 <> null;
SELECT * FROM null_table WHERE 1 < null;
SELECT * FROM null_table WHERE 1 > null;

-- Expect all rows
SELECT * FROM null_table WHERE null IS NULL;

-- Expect 0 rows
SELECT * FROM null_table WHERE null IS NOT NULL;
SELECT * FROM null_table WHERE null = null;
SELECT * FROM null_table WHERE null <> null;
SELECT * FROM null_table WHERE null > null;
SELECT * FROM null_table WHERE null < null;

-- Expect 0 rows
SELECT * FROM null_table WHERE 'a' IS NULL;

-- Expect all rows
SELECT * FROM null_table WHERE 'a' IS NOT NULL;

-- Expect 0 rows
SELECT * FROM null_table WHERE null = 'a';
SELECT * FROM null_table WHERE 'a' = null;
SELECT * FROM null_table WHERE 'a' <> null;
SELECT * FROM null_table WHERE 'a' > null;
SELECT * FROM null_table WHERE 'a' < null;

-- Expect 0 rows
SELECT * FROM null_table WHERE '2021-01-31' IS NULL;

-- Expect all rows
SELECT * FROM null_table WHERE '2021-01-31' IS NOT NULL;

-- Expect 0 rows
SELECT * FROM null_table WHERE null = '2021-01-31';
SELECT * FROM null_table WHERE '2021-01-31' = null;
SELECT * FROM null_table WHERE '2021-01-31' > null;
SELECT * FROM null_table WHERE '2021-01-31' < null;

-- 4. column predicates
-- Expect rows with non-null birthday: id = 1,2,4
SELECT * FROM null_table WHERE birthday IS NOT NULL;

-- Expect row with null birthday: id = 3
SELECT * FROM null_table WHERE birthday IS NULL;

-- Expect 0 rows
SELECT * FROM null_table WHERE birthday = null;
SELECT * FROM null_table WHERE null = birthday;
SELECT * FROM null_table WHERE birthday <> null;
SELECT * FROM null_table WHERE birthday > null;
SELECT * FROM null_table WHERE birthday < null;

-- Expect rows with non-null num: id = 1,3,4
SELECT * FROM null_table WHERE num IS NOT NULL;

-- Expect row with null num: id = 2
SELECT * FROM null_table WHERE num IS NULL;

-- Expect 0 rows
SELECT * FROM null_table WHERE num = null;
SELECT * FROM null_table WHERE null = num;
SELECT * FROM null_table WHERE num <> null;
SELECT * FROM null_table WHERE num > null;
SELECT * FROM null_table WHERE num < null;

-- 5. join behavior with null join keys
-- Expect only one row from num = 18 match; rows with NULL num should not join
SELECT null_table.num, null_table2.num, null_table.birthday
FROM null_table, null_table2
WHERE null_table.num = null_table2.num;

-- 6. aggregation
-- Expect 4
SELECT count(*) FROM null_table;

-- Expect 4 because price is always non-null in committed rows
SELECT count(price) FROM null_table;

-- Expect 3
SELECT count(birthday) FROM null_table;

-- Expect 15 because (18 + 12 + 15) / 3 = 15
SELECT avg(num) FROM null_table;

-- 7. all-null aggregation
CREATE TABLE null_table3(id int not null, num int null);
INSERT INTO null_table3 VALUES (1, null);
INSERT INTO null_table3 VALUES (2, null);

-- Expect 0
SELECT count(num) FROM null_table3;

-- Expect NULL
SELECT min(num) FROM null_table3;
SELECT max(num) FROM null_table3;
SELECT avg(num) FROM null_table3;

-- 8. update path
UPDATE null_table SET num = null WHERE id = 4;
SELECT * FROM null_table WHERE num IS NULL;
SELECT count(num) FROM null_table;

UPDATE null_table SET num = 99 WHERE id = 2;
SELECT * FROM null_table WHERE id = 2;
SELECT * FROM null_table WHERE num = 99;

-- Expect failure: set NOT NULL column to NULL
UPDATE null_table SET price = null WHERE id = 1;

-- 9. index-maintenance smoke check
UPDATE null_table SET num = null WHERE id = 1;
UPDATE null_table SET num = 18 WHERE id = 1;
SELECT * FROM null_table WHERE num IS NULL;
SELECT * FROM null_table WHERE num = 18;
