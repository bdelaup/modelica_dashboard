# build.ps1 - rejoue en local (Windows) la sequence exacte de la CI
# (.github/workflows/build.yml) : build -> ctest -> contrat -> package.
# ADR-0008/D18, ADR-0015 : plus de WebView2/nuget, le rendu et l'edition
# se font dans le navigateur via le dashboard manager.
# Prerequis : MSVC/CMake, Python 3. Fichier volontairement en ASCII pur (piège
# d'encodage UTF-8 sans BOM de Windows PowerShell 5.1, voir docs/contributeur.md).
$ErrorActionPreference = 'Stop'
$ROOT = Split-Path -Parent $PSScriptRoot
Set-Location $ROOT

Write-Host "== [1/4] configure =="
cmake -S $ROOT -B "$ROOT\build" -DCMAKE_BUILD_TYPE=Release

Write-Host "== [2/4] build (shared + modelica/csrc + tests) =="
cmake --build "$ROOT\build" -j --config Release

Write-Host "== [3/4] tests unitaires C++ + integration (CTest) =="
$env:DASHBOARD_REPO_ROOT = $ROOT
ctest --test-dir "$ROOT\build" --output-on-failure --build-config Release

Write-Host "== [4/4] tests de contrat (golden-file) puis packaging =="
python "$ROOT\tests\contract\test_contract.py"

New-Item -ItemType Directory -Force "$ROOT\dist" | Out-Null
Copy-Item "$ROOT\build\modelica\csrc\Release\DashboardExternal.dll" "$ROOT\dist\" -ErrorAction SilentlyContinue
# Package Modelica complet (Resources/Include + Resources/Library/win64/DashboardExternal.dll
# installee en post-build par CMake depuis modelica/csrc).
Copy-Item -Recurse -Force "$ROOT\modelica\Dashboard" "$ROOT\dist\Dashboard"
Write-Host "BUILD OK - artefacts dans dist\ (DashboardExternal.dll, Dashboard\)"
