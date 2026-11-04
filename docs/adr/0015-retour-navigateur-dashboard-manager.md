# ADR-0015 — Retour arrière : rendu et édition dans le navigateur, « pivot » renommé **dashboard manager**

**Statut** : **acté le 2026-11-04** (points validés avec le demandeur — voir § « Décisions validées »)
**Références** : [spec technique](../spec_technique.md), [protocole du canal](../protocole.md), [options d'architecture](../options_archtecture.md) (A2), ADR-0000/0001/0005/0007/0013 (remis en cause — statuts annotés)

## Contexte

L'architecture A4b (ADR-0000/0001) reposait sur un binaire `viewer.exe` embarquant WebView2, l'IHM en `app://` et draw.io, avec le SDK WebView2 installé via NuGet (`.nuget/`). Décision du 2026-11-04 : **le rendu et l'édition se font dans le navigateur du poste**. Objectif pour la V1 : simplifier et alléger — plus de dépendance WebView2/NuGet, une UI web standard testable en CI, un seul composant côté affichage.

## Décisions

- **Architecture retenue : variante de A2** (« web locale zéro install » ouverte dans le navigateur du poste), avec placement **P2** : le pivot devient un **processus autonome**, le **dashboard manager** — ni thread de la sim (P1), ni embarqué dans un binaire d'affichage (P3).
- **Deux ports loopback uniquement** : canal sim ↔ manager `127.0.0.1:51234` (inchangé, D26) + UI `127.0.0.1:51235` **fixe** (D45).
- **Le navigateur est l'IHM** : viewer temps réel (rendu R3 hybride, D8) + éditeur draw.io self-hosté (mode embed, D6) servis par le dashboard manager et ouverts sur `http://127.0.0.1:51235`.
- **Suppression du moteur de rendu embarqué** (fait le 2026-11-04) : `viewer/` (hôte WebView2, shell headless, UI embarquée, tests), `tests/integration/test_headless_e2e.py`, et les artefacts locaux `.nuget/`, `build/`, `dist/`.
- **Protocole** : le canal sim ↔ manager est **inchangé** (version 0 maintenue — le contrat n'a jamais été releasé) ; seul le transport du segment manager ↔ UI change (pont WebView2 → SSE/POST), le modèle de messages de `protocole.md` §7 est inchangé.
- **Sécurité** : loopback strict conservé, plus **validation de l'en-tête `Host`** (`127.0.0.1` / `localhost` uniquement) côté serveur UI — mesure anti DNS-rebinding rendue nécessaire par le navigateur (à reporter dans l'ADR-0011).

## Décisions validées le 2026-11-04 (avec le demandeur)

| # | Point | Décision actée |
|---|---|---|
| 1 | Architecture cible | Variante de A2, manager autonome (P2), navigateur = IHM, ports 51234 + 51235 |
| 2 | Port UI | **Fixe `127.0.0.1:51235`** (cohérent avec le port fixe du canal D5/D26) |
| 3 | Canal UI sur 51235 | **HTTP statique** (assets + `GET /api/probe`) + **SSE** manager→UI (`tick`, `simState`, `unconnected`, `dashboard`) + **POST** UI→manager (`cmd`, `loadDashboard`, `reload`, `openEditor`). WebSocket NDJSON écarté en V1 (SSE+POST plus simple, flux dominant manager→UI) |
| 4 | Livrable V1 | **Script Python embarqué (PyInstaller)** → exe autonome `dashboard_manager.exe`, assets web embarqués, zéro installation (EF-E2). Le C++ de `shared/` est **conservé et buildé en parallèle** (« on essaie de maintenir les deux pour voir ») — les deux implémentations du contrat sont validées par les mêmes golden files `tests/contract/` |

## Conséquences

- Build allégé : plus de NuGet, plus de copie d'assets web à côté de l'exe, plus de tests JS embarqués dans un WebView.
- Le navigateur apporte devtools et l'indépendance OS de l'UI ; en contrepartie il faut un navigateur moderne sur le poste et la validation `Host` côté serveur.
- EF-E2 (« aucune installation ») reste satisfait par l'exe embarqué ; ENF-1 « à défaut » (web servie en local) est retenu.
- Le code C++ existant (`shared/`, `modelica/csrc/`, `tests/contract/`) reste valable et buildé ; le manager Python (`manager/`) réimplémente le même contrat de canal. La logique JS de l'UI (`manager/web/`) reste testable en Node (pont SSE/POST injecté).
