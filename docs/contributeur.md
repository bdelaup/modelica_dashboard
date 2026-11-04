# Guide du contributeur

**Projet** : modelica_dashboard (dashboard manager + navigateur — [ADR-0015](adr/0015-retour-navigateur-dashboard-manager.md))
**Références** : [spec technique](spec_technique.md) §7/§9, [protocole](protocole.md), [ADR](adr/) (décisions actées, dont [ADR-0013](adr/0013-decisions-mise-en-oeuvre.md) et [ADR-0015](adr/0015-retour-navigateur-dashboard-manager.md))

---

## 1. Prérequis

- **Linux/macOS (portage tests)** : g++/clang + CMake ≥ 3.20, Python 3 — build `shared/`, `modelica/csrc/`, tous les tests CTest (`shared_tests`, `end_to_end`, `bench_framing` 1000 var × 100 Hz)
- **Windows V1 (livrable)** : Visual Studio 2022 (Desktop C++, workload CMake) ou MSVC + CMake ≥ 3.28, en plus du pack ci-dessus
- **Plus de WebView2 ni de NuGet** (ADR-0015) : le rendu et l'édition se font dans le navigateur du poste, servi par le dashboard manager
- Node.js LTS (tests de la logique JS de l'UI web, `manager/web/`, quand elle sera recréée)
- OpenModelica **v1.27.1** (D27) avec `omc` dans le `PATH` (tests d'intégration OM, Windows)
- Python 3 (tests de contrat et E2E) + **PyInstaller** (packaging du manager en exe autonome, D47)

## 2. Arborescence

```
modelica_dashboard/
├── docs/                  # Zensical (ce dossier)
├── manager/               # dashboard manager (à développer — ADR-0015) : serveur loopback Python, packagé en exe (PyInstaller, D47)
│   ├── CMakeLists.txt
│   ├── src/               # canal sim 127.0.0.1:51234 + serveur HTTP+SSE/POST UI 127.0.0.1:51235 + routage
│   ├── web/               # UI navigateur (viewer + éditeur draw.io self-hosté) servie en loopback, logique JS testable en Node
│   └── tests/             # tests Catch2 du manager
├── modelica/              # package Modelica Dashboard + fonction C externe (DLL) — inchangé
│   ├── Dashboard/         # .mo (blocs, paramètres)
│   └── csrc/              # fonction C externe, build propre (DLL DashboardExternal.dll)
├── shared/                # cœur repris par le manager : constantes (port 51234, version 0), protocole NDJSON,
│   │                      # parsing .drawio, pivot loopback (renommé dashboard_manager au dev — ADR-0015)
│   └── CMakeLists.txt     # bibliothèque statique partagée manager ↔ tests
├── tests/                 # intégration (harness C + end_to_end + bench) + contrat (golden-file)
└── scripts/               # build.ps1 / build.sh : rejoue la séquence exacte de la CI en local
```

Règles de cloisonnement (spec §7) : `manager/`, `modelica/`, `shared/` ne se référencent que via `shared/` ; la logique JS de l'UI (`manager/web/`) n'utilise aucune API navigateur propriétaire (pont SSE/POST injecté, testable en Node).

## 3. Build et tests en local (rejouable, identique à la CI)

```powershell
.\scripts\build.ps1        # Windows
./scripts/build.sh          # Linux/macOS
```

Séquence exacte (ADR-0008, D18) :

1. build de chaque composant (`cmake --build` dans `shared/`, `modelica/csrc/`, `tests/`)
2. tests unitaires/intégration C++ : `ctest` (`shared_tests`, `end_to_end` — harness C + manager réel sur 51234 —, `bench_framing` 1000 var × 100 Hz, ADR-0003)
3. tests de contrat (golden-file) : `tests/contract/`
4. packaging dans `dist/` : `DashboardExternal.dll` + `Dashboard/` (Windows) ; binaires de test (Linux)

Quand le manager et son UI seront développés (ADR-0015), `manager/` (Python : tests pytest + E2E via SSE/POST + packaging PyInstaller) et `manager/web/` (tests JS `node --test`) s'ajouteront à cette séquence.

**Encodage des scripts** : `scripts/build.ps1` est volontairement en **ASCII pur** (CRLF), ce qui évite le piège UTF-8 sans BOM de Windows PowerShell 5.1 (voir git history / ADR-0013) — ne pas y réintroduire d'accents.

### 3.1 Monter un environnement Windows from scratch (vérifié le 2026-11-04)

Checklist reproductible (Windows 11, VS 2022 Community, CMake 3.30, Python 3.12) :

1. **Visual Studio 2022** (Community suffisant) avec le workload « Développement Desktop C++ » (apporte MSVC + CMake).
2. **Node.js LTS** et **Python 3** dans le `PATH` (`node --version`, `python --version`).
3. **Ouvrir « Developer PowerShell for VS 2022 »** (menu Démarrer) pour avoir `cl` dans le `PATH`, puis :
   ```powershell
   .\scripts\build.ps1
   ```

**Résultat attendu** (référence) : configure OK → build OK → `ctest` **3/3 passés** (`shared_tests`, `end_to_end`, `bench_framing`) → contrat **CONTRAT OK** → `BUILD OK`, artefacts dans `dist\` (`DashboardExternal.dll`, `Dashboard\`).

### 3.2 Compiler un modèle avec omc sans polluer le dépôt (vérifié le 2026-11-03)

**Règle absolue : ne jamais lancer `omc` depuis la racine du dépôt.** omc génère ~40 fichiers dans le répertoire courant (`<Model>.c`, `<Model>.o`, `<Model>.makefile`, `<Model>.exe`, `<Model>_01exo.c`, …). Toujours travailler dans **`build/omc/`** (couvert par `build/` dans `.gitignore`) ; des patterns racine-only dans `.gitignore` servent de filet de sécurité.

```powershell
# 1. dossier de travail dédié + la DLL et son import lib
New-Item -ItemType Directory -Force build\omc | Out-Null
Copy-Item build\modelica\csrc\Release\DashboardExternal.dll, build\modelica\csrc\Release\DashboardExternal.lib build\omc\

# 2. omc lancé DEPUIS build/omc, avec des chemins absolus dans le .mos
Set-Location build\omc
& "D:\programmes\OpenModelica1.27.1-64bit\bin\omc.exe" build_poc.mos
#   build_poc.mos : loadFile("D:/gits/modelica_dashboard/modelica/Dashboard/Examples/Demo.mo"); buildModel(Demo);
#   annotations du modèle : Library="DashboardExternal",
#   LibraryDirectory="file:///D:/gits/modelica_dashboard/build/omc",
#   Include="#include \"D:/gits/modelica_dashboard/modelica/csrc/dashboard_client.h\""
Set-Location ..\..

# 3. lancer l'exe généré avec le bin d'OM dans le PATH (libSimulationRuntimeC.dll),
#    AVEC le dashboard manager lancé avant (canal 127.0.0.1:51234)
$env:PATH = "D:\programmes\OpenModelica1.27.1-64bit\bin;" + $env:PATH
.\build\omc\Demo.exe
```

Vérification : `git status --short` doit rester vide (hors modifications volontaires de sources) après compile + run omc.

Pièges omc v1.27.1 constatés (voir aussi [docs/poc-binding-runtime.md](poc-binding-runtime.md)) : `omc` hors `PATH` par défaut (winget installe dans `d:\programmes\OpenModelica1.27.1-64bit`) ; chaînes `"""..."""` non parsées dans `Include` (utiliser `\"...\\n...\"`) ; `external "C"` interdit directement dans un `block` (passer par des `function`) ; arguments externes = identificateurs uniquement.

## 4. Contrat du canal et golden files

- Le contrat est dans [protocole.md](protocole.md) — rédigé avant le code (D29).
- Toute évolution du format `.drawio` ou du protocole met à jour `protocole.md`, `shared/`, **et** régénère les golden files de `tests/contract/` ; le test de contrat échoue volontairement sinon (versionnage explicite, ADR-0009).
- Versions de départ : produit `v0.1.0`, protocole `version: 0`, `dashboardVersion=\"0\"` (D31). L'ADR-0015 maintient le protocole à la version 0 (le canal sim ↔ manager est inchangé ; seul le transport manager ↔ navigateur est passé de WebView2 à SSE/POST).

## 5. CI

Workflow `.github/workflows/build.yml` : job `linux` (ubuntu : build + ctest + contrat) et job `windows` (build + ctest + contrat + packaging `dist/` + release GitHub sur tag `v*.*.*`). Plus de WebView2/NuGet depuis l'ADR-0015. La doc (`docs/`) est déployée sur GitHub Pages par le workflow `docs.yml` à chaque merge sur `main`.

## 6. Pull requests

- Branche `vibe/<sujet>-<id>`, PR vers `main`, description : summary + vérification (commandes lancées).
- Toute nouvelle décision structurante = nouvel ADR dans `docs/adr/` + entrée dans [l'index](adr/index.md) et dans le tableau de la [spec technique](spec_technique.md) §0.
