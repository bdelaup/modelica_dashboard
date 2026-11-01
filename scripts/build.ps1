# build.ps1 — rejoue en local (Windows) la séquence exacte de la CI
# (.github/workflows/build.yml) : build -> ctest -> node --test -> contrat -> package.
# ADR-0008/D18. Prérequis : MSVC/CMake, Node LTS, Python 3, WebView2 runtime.
$ErrorActionPreference = 'Stop'
$ROOT = Split-Path -Parent $PSScriptRoot
Set-Location $ROOT

Write-Host "== [1/6] configure =="
cmake -S $ROOT -B "$ROOT\build" -DCMAKE_BUILD_TYPE=Release

Write-Host "== [2/6] build (shared + modelica/csrc + viewer.exe + tests) =="
cmake --build "$ROOT\build" -j --config Release

Write-Host "== [3/6] tests unitaires C++ + intégration (CTest) =="
$env:DASHBOARD_REPO_ROOT = $ROOT
ctest --test-dir "$ROOT\build" --output-on-failure --build-config Release

Write-Host "== [4/6] tests JS du viewer (node --test) =="
Push-Location "$ROOT\viewer\src\web"
npm test --silent
Pop-Location

Write-Host "== [5/6] tests de contrat (golden-file) =="
python "$ROOT\tests\contract\test_contract.py"

Write-Host "== [6/6] packaging =="
New-Item -ItemType Directory -Force "$ROOT\dist" | Out-Null
Copy-Item "$ROOT\build\viewer\Release\viewer.exe" "$ROOT\dist\" -ErrorAction SilentlyContinue
Copy-Item "$ROOT\build\modelica\csrc\Release\DashboardExternal.dll" "$ROOT\dist\" -ErrorAction SilentlyContinue
Copy-Item -Recurse "$ROOT\modelica\Dashboard" "$ROOT\dist\Dashboard"
Write-Host "BUILD OK — artefacts dans dist\ (viewer.exe, DashboardExternal.dll, Dashboard\)"
