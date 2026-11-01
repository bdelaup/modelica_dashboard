# Guide du contributeur

**Projet** : modelica_dashboard (architecture A4b)
**Références** : [spec technique](spec_technique.md) §7/§9, [protocole](protocole.md), [ADR](adr/) (décisions actées, dont [ADR-0013](adr/0013-decisions-mise-en-oeuvre.md) pour les décisions de mise en œuvre)

---

## 1. Prérequis (poste Windows, V1)

- Visual Studio 2022 (Desktop C++, workload CMake) ou MSVC + CMake ≥ 3.28
- WebView2 runtime (présent par défaut sur Windows 10/11)
- Node.js LTS (tests de la logique JS du viewer)
- OpenModelica **v1.27.1** (D27) avec `omc` dans le `PATH` (tests d'intégration)
- Python 3 (scripts de test d'intégration)

## 2. Arborescence (à créer au fil du dev)

```
modelica_dashboard/
├── docs/                  # Zensical (ce dossier)
├── viewer/                # binaire C++/Win32 : pivot embarqué + WebView2 + IHM embarquée (app://) + draw.io embarqué
│   ├── CMakeLists.txt
│   ├── src/cpp/           # pivot (serveur loopback), hôte WebView2, pont natif, iframe éditeur
│   ├── src/web/           # bibliothèque JS du viewer (testable en Node) + assets IHM embarqués
│   └── third_party/       # draw.io v32.3.0 (build statique webapp), nlohmann/json, Catch2
├── modelica/              # package Modelica Dashboard + fonction C externe (DLL)
│   ├── Dashboard/         # .mo (blocs, paramètres)
│   └── csrc/              # fonction C externe, build propre (DLL DashboardExternal.dll)
├── shared/                # contrat commun : constantes (port 51234, version 0), schéma/validation du protocole, parsing .drawio
│   └── CMakeLists.txt     # bibliothèque statique partagée viewer ↔ tests
├── tests/                 # unitaires + intégration + contrat (golden-file)
└── scripts/               # build.ps1 / build.sh : rejoue la séquence exacte de la CI en local
```

Règles de cloisonnement (spec §7) : `viewer/`, `modelica/`, `shared/` ne se référencent que via `shared/` ; la logique JS du viewer (`viewer/src/web/`) n'utilise aucune API WebView2 (pont injecté, testable en Node).

## 3. Build et tests en local (rejouable, identique à la CI)

```powershell
.\scripts\build.ps1
```

Séquence exacte (ADR-0008, D18) :

1. build de chaque composant (`cmake --build` dans `shared/`, `viewer/`, `modelica/csrc/`)
2. tests unitaires C++ : `ctest` (Catch2 v3, D33)
3. tests unitaires JS : `node --test` dans `viewer/src/web/`
4. tests d'intégration : modèle de démo compilé par `omc`, flux réel sur `127.0.0.1:51234` (handshake, data, commandes, modes auto/manuel, EF-B4/EF-V6)
5. tests de contrat (golden-file) : `tests/contract/`
6. packaging de `viewer.exe`

Le POC du cœur risqué (D28 : binding par nom, commandes, temps réel) est le premier incrément de `modelica/csrc/` + `tests/integration/`.

## 4. Contrat du canal et golden files

- Le contrat est dans [protocole.md](protocole.md) — rédigé avant le code (D29).
- Toute évolution du format `.drawio` ou du protocole met à jour `protocole.md`, `shared/`, **et** régénère les golden files de `tests/contract/` ; le test de contrat échoue volontairement sinon (versionnage explicite, ADR-0009).
- Versions de départ : produit `v0.1.0`, protocole `version: 0`, `dashboardVersion="0"` (D31).

## 5. CI

Workflow `.github/workflows/build.yml` sur `windows-latest` : mêmes étapes que `scripts/build.ps1` (build → ctest → `node --test` → intégration → contrat → package → artifact). Release GitHub sur tag `v*.*.*` (ADR-0008).

## 6. Pull requests

- Branche `vibe/<sujet>-<id>`, PR vers `main`, description : summary + vérification (commandes lancées).
- Toute nouvelle décision structurante = nouvel ADR dans `docs/adr/` + entrée dans [l'index](adr/index.md) et dans le tableau de la [spec technique](spec_technique.md) §0.
- La doc (`docs/`) est déployée sur GitHub Pages par le workflow existant `docs.yml` à chaque merge sur `main`.
