#!/usr/bin/env python3
"""Generate deterministic big-query stress SQL (seed=7)."""
import random
import sys

SEED = 7
ROWS_MAIN = 800
ROWS_DIM = 20

random.seed(SEED)

lines = [
    "CREATE TABLE bq_main(id INT, k INT, v INT);",
    "CREATE TABLE bq_dim(k INT, label INT);",
    "CREATE INDEX idx_bq_main_k ON bq_main(k);",
    "CREATE INDEX idx_bq_main_id ON bq_main(id);",
]

for i in range(1, ROWS_MAIN + 1):
    k = i % ROWS_DIM
    lines.append(f"INSERT INTO bq_main VALUES ({i}, {k}, {i * 3});")

for k in range(ROWS_DIM):
    lines.append(f"INSERT INTO bq_dim VALUES ({k}, {k * 100});")

for rid in [1, 42, 400, ROWS_MAIN]:
    lines.append(f"SELECT id, v FROM bq_main WHERE id={rid};")

for k in [0, 7, 19]:
    lines.append(f"SELECT count(*) FROM bq_main WHERE k={k};")

lines.append("SELECT k, count(*), sum(v) FROM bq_main GROUP BY k;")
lines.append("SELECT k, avg(v) FROM bq_main GROUP BY k HAVING count(*) > 30;")
lines.append("SELECT id FROM bq_main WHERE k IN (SELECT k FROM bq_dim WHERE label > 500) ORDER BY id;")
lines.append("SELECT count(*) FROM bq_main, bq_dim WHERE bq_main.k = bq_dim.k;")

for _ in range(15):
    rid = random.randint(1, ROWS_MAIN)
    lines.append(f"SELECT v FROM bq_main WHERE id={rid};")

lines.append("SELECT count(*) FROM bq_main;")

print("\n".join(lines))
print(f"-- expected_rows={ROWS_MAIN}", file=sys.stderr)
