#!/usr/bin/env bash
# build.sh - rejoue en local la sequence exacte de la CI (.github/workflows/build.yml)
# ADR-0008/D18, ADR-0015 : plus de WebView2, rendu/edition dans le navigateur.
# Usage : ./scripts/build.sh   (Linux/macOS : shared + csrc + tests).
set -euo pipefail
cd "$(dirname "$0")/.."
ROOT="$(pwd)"

echo "== [1/4] configure =="
cmake -S "$ROOT" -B "$ROOT/build" -DCMAKE_BUILD_TYPE=Release

echo "== [2/4] build (shared + modelica/csrc + tests) =="
cmake --build "$ROOT/build" -j

echo "== [3/4] tests C++ (CTest : shared, integration end_to_end, benchmark framing) =="
DASHBOARD_REPO_ROOT="$ROOT" ctest --test-dir "$ROOT/build" --output-on-failure

echo "== [4/4] tests de contrat (golden-file) + packaging (artefacts de test) =="
python3 "$ROOT/tests/contract/test_contract.py"

mkdir -p "$ROOT/dist"
cp "$ROOT/build/shared/shared_tests" "$ROOT/dist/" 2>/dev/null || true
cp "$ROOT/build/tests/integration/test_end_to_end" "$ROOT/dist/" 2>/dev/null || true
cp "$ROOT/build/modelica/csrc/dashboard_harness" "$ROOT/dist/" 2>/dev/null || true
echo "BUILD OK - artefacts dans dist/"
