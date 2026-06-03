-- complex-sub-query step 1: nested non-correlated subqueries (no outer refs)
CREATE TABLE csq_1(id int, col1 int, feat1 float);
CREATE TABLE csq_2(id int, col2 int, feat2 float);
CREATE TABLE csq_3(id int, col3 int, feat3 float);
INSERT INTO csq_1 VALUES (1, 4, 11.2);
INSERT INTO csq_1 VALUES (2, 2, 12.0);
INSERT INTO csq_1 VALUES (3, 3, 13.5);
INSERT INTO csq_2 VALUES (1, 2, 13.0);
INSERT INTO csq_2 VALUES (2, 7, 10.5);
INSERT INTO csq_2 VALUES (5, 3, 12.6);
INSERT INTO csq_3 VALUES (1, 2, 11.0);
INSERT INTO csq_3 VALUES (3, 6, 16.5);
INSERT INTO csq_3 VALUES (5, 5, 14.6);
-- nested IN / NOT IN
SELECT * FROM csq_1 WHERE id IN (SELECT csq_2.id FROM csq_2 WHERE csq_2.id IN (SELECT csq_3.id FROM csq_3));
SELECT * FROM csq_1 WHERE id IN (SELECT csq_2.id FROM csq_2 WHERE csq_2.id NOT IN (SELECT csq_3.id FROM csq_3));
SELECT * FROM csq_1 WHERE col1 NOT IN (SELECT csq_2.col2 FROM csq_2 WHERE csq_2.id NOT IN (SELECT csq_3.id FROM csq_3));
SELECT * FROM csq_1 WHERE col1 NOT IN (SELECT csq_2.col2 FROM csq_2 WHERE csq_2.id IN (SELECT csq_3.id FROM csq_3));
-- nested scalar comparison
SELECT * FROM csq_1 WHERE col1 > (SELECT avg(csq_2.col2) FROM csq_2 WHERE csq_2.feat2 >= (SELECT min(csq_3.feat3) FROM csq_3));
SELECT * FROM csq_1 WHERE (SELECT avg(csq_2.col2) FROM csq_2 WHERE csq_2.feat2 > (SELECT min(csq_3.feat3) FROM csq_3)) = col1;
SELECT * FROM csq_1 WHERE (SELECT avg(csq_2.col2) FROM csq_2) <> (SELECT avg(csq_3.col3) FROM csq_3);
-- multiple subqueries in one predicate
SELECT * FROM csq_1 WHERE feat1 > (SELECT min(csq_2.feat2) FROM csq_2) AND col1 <= (SELECT min(csq_3.col3) FROM csq_3);
SELECT * FROM csq_1 WHERE (SELECT max(csq_2.feat2) FROM csq_2) > feat1 AND col1 > (SELECT min(csq_3.col3) FROM csq_3);
SELECT * FROM csq_1 WHERE (SELECT max(csq_2.feat2) FROM csq_2) > feat1 AND (SELECT min(csq_3.col3) FROM csq_3) < col1;
-- empty inner / nested empty subqueries
SELECT * FROM csq_1 WHERE id IN (SELECT csq_2.id FROM csq_2 WHERE csq_2.id IN (SELECT csq_3.id FROM csq_3 WHERE 1=0));
SELECT * FROM csq_1 WHERE id IN (SELECT csq_2.id FROM csq_2 WHERE csq_2.id IN (SELECT csq_3.id FROM csq_3 WHERE 1=0) AND 1=0);
SELECT * FROM csq_1 WHERE col1 NOT IN (SELECT csq_2.col2 FROM csq_2 WHERE csq_2.id NOT IN (SELECT csq_3.id FROM csq_3 WHERE 1=0));
SELECT * FROM csq_1 WHERE col1 NOT IN (SELECT csq_2.col2 FROM csq_2 WHERE csq_2.id NOT IN (SELECT csq_3.id FROM csq_3) AND 1=0);
SELECT * FROM csq_3 WHERE feat3 < (SELECT max(csq_2.feat2) FROM csq_2 WHERE csq_2.id NOT IN (SELECT csq_3.id FROM csq_3 WHERE 1=0));
SELECT * FROM csq_3 WHERE feat3 < (SELECT max(csq_2.feat2) FROM csq_2 WHERE csq_2.id NOT IN (SELECT csq_3.id FROM csq_3) AND 1=0);
-- correlated subqueries (step 2)
SELECT * FROM csq_1 WHERE feat1 <> (SELECT avg(csq_2.feat2) FROM csq_2 WHERE csq_2.feat2 > csq_1.feat1);
SELECT * FROM csq_1 WHERE col1 NOT IN (SELECT csq_2.col2 FROM csq_2 WHERE csq_2.id IN (SELECT csq_3.id FROM csq_3 WHERE csq_1.id = csq_3.id));
