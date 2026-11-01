# ADR-0013 — Décisions de mise en œuvre (session de cadrage dev)

**Statut** : acté
**Références** : [spec technique](../spec_technique.md) (D26–D34), [protocole](../protocole.md), besoin (EF/ENF)

## Contexte

La spec technique A4b (ADR-0001…0012) laissait des points ouverts bloquants pour démarrer le dev : valeurs concrètes (port, versions), périmètre V1 des widgets, stack outillage, mécanisme réel de l'éditeur, et ordre de travail. Ces points ont été tranchés avec le demandeur.

## Décisions

| # | Sujet | Décision |
|---|---|---|
| D26 | Port fixe du canal | `127.0.0.1:51234` (choix laissé au dev ; hors plages usuelles, documenté dans `shared/` et [protocole.md](../protocole.md)) |
| D27 | OpenModelica cible | **v1.27.1** (dernière release stable au cadrage) épinglée pour la bibliothèque Modelica, la DLL C et la CI |
| D28 | Cœur risqué | **POC/spike en parallèle du socle** : résolution par nom via l'API runtime, commandes run/pause/runToEnd depuis la DLL, synchro temps réel (option A6 de `options_archtecture.md` comme premier incrément) |
| D29 | Contrat du canal | `docs/protocole.md` rédigé **avant le code** ; code, golden files et tests s'y conforment |
| D30 | Mécanisme de l'éditeur | **draw.io en iframe dans le WebView2 du viewer**, servi via `app://` (ressources embarquées), piloté par `postMessage` — **plus de navigateur externe, plus de mini-serveur HTTP** (résout la contradiction §5 vs §8.6 de la spec ; corrige ADR-0004) |
| D31 | Versions de départ | Produit **v0.1.0**, `version` du protocole = **0**, `dashboardVersion` du fichier = **0** |
| D32 | Widgets V1 | `value`, `gauge`, `curve`, `button`, `slider`, `log` — `text`, `image`, `table` reportés en V2 |
| D33 | Stack C++ | **C++20 + CMake**, JSON via **nlohmann/json** (header-only, `third_party/`), tests **Catch2 v3** via CTest ; logique JS du viewer testée en **Node LTS** (`node --test`, pont simulé) |
| D34 | draw.io embarqué | **v32.3.0** (dernier tag stable au cadrage, octobre 2026), build statique `webapp` embarqué dans `viewer/third_party/` |
| D35 | Taille max de trame | **1 MiB** (1 048 576 octets) par ligne NDJSON sur le canal |
| D36 | Langue UI par défaut | **Français** (sélecteur FR/EN conservé, D21) |
| D37 | Logs | `%LOCALAPPDATA%\modelica_dashboard\logs\{viewer,pivot,sim}.log` (Windows V1) |
| D38 | DLL C externe | Nom `DashboardExternal.dll`, fonctions exportées préfixées `dashboard_` (contrat C détaillé au POC, D28) |
| D39 | `showPort` (EF-B2) | Mécanisme visé : `Placement(visible=showPort)` sur le connecteur du bloc — **à valider au POC** ; si OMEdit ne l'évalue pas sans plugin, repli = bloc à port masqué par défaut, `showPort` lu uniquement par le pivot pour la liste des variables |
| D40 | Message d'état sim→pivot | Ajout d'un message `state` sim→pivot (absent du contrat initial) pour alimenter `simState` côté viewer |
| D41 | Ordre de dev | (1) `protocole.md` + contrat `shared/` → (2) socle `shared/` + tests de contrat + `scripts/build.ps1` + CI → (3) POC canal/binding **en parallèle** → (4) pivot C++ → (5) bibliothèque Modelica + DLL → (6) viewer WebView2 + logique JS → (7) éditeur iframe |

## Conséquences

- D30 simplifie la sécurité : il ne reste **qu'un seul port ouvert** (le canal, 51234) ; l'éditeur n'ouvre aucun port, la §10 de la spec est mise à jour en conséquence.
- D31 : contrats à `version: 0` — toute évolution cassante passera le protocole à 1 (et `dashboardVersion` à 1), la politique N-1 de l'ADR-0009 s'applique dès la V1.
- D32 : la palette `<mxlibrary>` V1 ne contient que les 6 formes retenues.
- Le POC (D28) peut invalider D38/D39 : le contrat C de la DLL et le mécanisme `showPort` seront alors ajustés et consignés dans un ADR dédié.
