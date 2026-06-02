#!/usr/bin/env bash
# 全量回归 + 压力测试 — 在仓库根目录执行:
#   bash lab/test/run_all.sh
#   bash lab/test/run_all.sh --quick
#   bash lab/test/run_all.sh --build
set -euo pipefail

SCRIPT_DIR="$(cd "$(dirname "${BASH_SOURCE[0]}")" && pwd)"
REPO_ROOT="$(cd "${SCRIPT_DIR}/../.." && pwd)"

cd "${REPO_ROOT}"
exec python3 "${SCRIPT_DIR}/run_all.py" "$@"
