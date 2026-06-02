#!/usr/bin/env python3
"""Generate deterministic big-write stress SQL (seed=42)."""
import random

SEED = 42
INSERT_N = 500
UPDATE_N = 200
DELETE_N = 80

random.seed(SEED)

lines = [
    "CREATE TABLE big_write(id int, val int, tag char);",
    "CREATE INDEX idx_bw_id ON big_write(id);",
    "CREATE INDEX idx_bw_val ON big_write(val);",
]

ids = set(range(1, INSERT_N + 1))
for i in sorted(ids):
    lines.append(f"INSERT INTO big_write VALUES ({i}, {i * 2}, 't{i % 50}');")

for _ in range(UPDATE_N):
    rid = random.randint(1, INSERT_N)
    new_val = random.randint(1000, 9999)
    lines.append(f"UPDATE big_write SET val={new_val} WHERE id={rid};")

for rid in sorted(random.sample(list(ids), DELETE_N)):
    lines.append(f"DELETE FROM big_write WHERE id={rid};")
    ids.discard(rid)

lines.append("SELECT count(*) FROM big_write;")
for rid in sorted(list(ids))[:3]:
    lines.append(f"SELECT id, val FROM big_write WHERE id={rid};")
for rid in sorted(list(ids))[-3:]:
    lines.append(f"SELECT id, val FROM big_write WHERE id={rid};")

print("\n".join(lines))
print(f"-- expected_rows={len(ids)}", file=__import__("sys").stderr)
