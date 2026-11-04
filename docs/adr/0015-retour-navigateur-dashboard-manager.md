# ADR-0015 — Retour arrière : rendu et édition dans le navigateur, « pivot » renommé **dashboard manager**

**Statut** : proposé le 2026-11-04 — en attente de validation des points ouverts (§ « Points soumis à validation")
**Références** : [spec technique](../spec_technique.md), [protocole du canal](../protocole.md), [options d'architecture](../options_archtecture.md) (A2), ADR-0000/0001/0005/0007/0013 (remis en cause — statuts annotés)

## Contexte

L'architecture A4b (ADR-0000/0001) reposait sur un binaire `viewer.exe` embarquant WebView2, l'IHM en `app://` et draw.io, avec le SDK WebView2 installé via NuGet (`.nuget/`). Décision du 2026-11-04 : **le rendu et l'édition se font dans le navigateur du poste**. Objectif pour la V1 : simplifier et alléger — plus de dépendance WebView2/NuGet, une UI web standard testable en CI, un seul composant côté affichage.

## Décisions

- **Architecture retenue : variante de A2** (« web locale zéro install » ouverte dans le navigateur du poste), avec placement **P2** : le pivot devient un **processus autonome**, le **dashboard manager** — ni thread de la sim (P1), ni embarqué dans un binaire d'affichage (P3).
- **Deux ports loopback uniquement** : canal sim ↔ manager `127.0.0.1:51234` (inchangé, D26) + UI `127.0.0.1:51235` (HTTP pour les assets statiques + WebSocket NDJSON pour les messages — à valider).
- **Le navigateur est l'IHM** : viewer temps réel (rendu R3 hybride, D8) + éditeur draw.io self-hosté (mode embed, D6) servis par le dashboard manager et ouverts sur `http://127.0.0.1:51235`.
- **Suppression du moteur de rendu embarqué** (fait le 2026-11-04) : `viewer/` (hôte WebView2, shell headless, UI embarquée, tests), `tests/integration/test_headless_e2e.py`, et les artefacts locaux `.nuget/`, `build/`, `dist/`. `shared/` (protocole, parsing `.drawio`, pivot loopback) est **conservé** et sera renommé `dashboard_manager` au développement ; `modelica/` (bibliothèque + DLL C) est inchangé.
- **Protocole** : le canal sim ↔ manager est **inchangé** (version 0 maintenue — le contrat n'a jamais été releasé) ; seul le transport du segment manager ↔ UI change (pont WebView2 → WebSocket), le modèle de messages de `protocole.md` §7 est inchangé.
- **Sécurité** : loopback strict conservé, plus **validation de l'en-tête `Host`** (`127.0.0.1` / `localhost` uniquement) côté serveur UI — mesure anti DNS-rebinding rendue nécessaire par le navigateur (à reporter dans l'ADR-0011).
- **Livraison V1** : `dashboard_manager.exe` autonome (C++20, assets web embarqués, zéro installation EF-E2) + package Modelica inchangé (D17). Windows-only V1 pour la bibliothèque/sim ; le manager reste portable (build + tests Linux en CI, comme D42).

## Conséquences

- Build allégé : plus de NuGet, plus de copie d'assets web à côté de l'exe, plus de tests JS embarqués dans un WebView.
- Le navigateur apporte devtools et l'indépendance OS de l'UI ; en contrepartie il faut un navigateur moderne sur le poste et la validation `Host` côté serveur.
- EF-E2 (« aucune installation ») reste satisfait par l'exe autonome ; ENF-1 « à défaut » (web servie en local) est retenu.
- Le code existant (`shared/`, `modelica/csrc/`, `tests/contract/`) reste valable ; seul le shell d'affichage est à refaire (`manager/` + UI web servie).

## Points soumis à validation

1. Port UI fixe `51235` (cohérent avec le port fixe du canal, D5/D26) vs port éphémère + URL imprimée au lancement.
2. Canal UI : un seul WebSocket NDJSON (messages de `protocole.md` §7 : `tick`, `simState`, `unconnected`, `dashboard`, `cmd`, `probe`…) + HTTP statique pour les assets, le tout sur le port UI.
3. Livrable du manager : exe C++ autonome avec assets embarqués (L2, cohérent avec `shared/`) vs script Python embarqué (L1).
4. Éditeur draw.io self-hosté embarqué (édition dans le navigateur) et rendu viewer R3 hybride conservés tels quels.
