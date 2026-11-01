#!/usr/bin/env bash
# build.sh — rejoue en local la séquence exacte de la CI (.github/workflows/build.yml)
# ADR-0008/D18. Usage : ./scripts/build.sh   (Linux/macOS pour shared+csrc+tests ;
# le viewer.exe Win32/WebView2 n'est buildé que sur Windows, via build.ps1).
set -euo pipefail
cd "$(dirname "$0")/.."
ROOT="$(pwd)"

echo "== [1/6] configure =="
cmake -S "$ROOT" -B "$ROOT/build" -DCMAKE_BUILD_TYPE=Release

echo "== [2/6] build (shared + modelica/csrc + tests) =="
cmake --build "$ROOT/build" -j

echo "== [3/6] tests unitaires C++ (CTest : shared_tests + intégration) =="
DASHBOARD_REPO_ROOT="$ROOT" ctest --test-dir "$ROOT/build" --output-on-failure

echo "== [4/6] tests JS du viewer (node --test) =="
(cd "$ROOT/viewer/src/web" && npm test --silent)

echo "== [5/6] tests de contrat (golden-file) =="
python3 "$ROOT/tests/contract/test_contract.py"

echo "== [6/6] packaging (hors Windows : artefacts de test uniquement) =="
mkdir -p "$ROOT/dist"
cp "$ROOT/build/shared/shared_tests" "$ROOT/dist/" 2>/dev/null || true
cp "$ROOT/build/tests/integration/test_end_to_end" "$ROOT/dist/" 2>/dev/null || true
cp "$ROOT/build/modelica/csrc/dashboard_harness" "$ROOT/dist/" 2>/dev/null || true
if [ -f "$ROOT/build/viewer/viewer.exe" ]; then cp "$ROOT/build/viewer/viewer.exe" "$ROOT/dist/"; fi
echo "BUILD OK — artefacts dans dist/"
