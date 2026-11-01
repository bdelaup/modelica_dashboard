# ADR-0004 — Éditeur draw.io self-hosté embed, format .drawio natif, rendu viewer R3 hybride, C2

**Statut** : acté (C2 à réévaluer)
**Références** : [options techniques](../option_technique.md) §3/§4/§7, [spec technique](../spec_technique.md) D6/D7/D8/D12

## Contexte

A4b impose un éditeur externe au viewer. Choix : draw.io self-hosté en embed vs éditeur custom. Le format du fichier (.drawio natif vs JSON/YAML) en découle. Le viewer doit rendre ce format en temps réel.

## Décision

- **Éditeur** : **draw.io self-hosté en mode embed** (`?embed=1&proto=json`), build statique embarqué dans le binaire et servi via `app://` dans une **iframe du WebView2** (D30, ADR-0013 — mécanisme corrigé : plus de navigateur externe ni de mini-serveur HTTP), piloté par `postMessage` (load/save). Palette de widgets EF-E1 via `<mxlibrary>` custom servies via `app://`.
- **Format** : **`.drawio` natif** (XML mxGraphModel), cellules `UserObject` typées (`type`, `variable`), champ `dashboardVersion` sur la racine. Fichier à côté du `.mo`, nom libre (EF-E3).
- **Rendu du viewer** : **R3 hybride** — moteur draw.io (`viewer-static.min.js`) pour la structure/layout, mise à jour des cellules via l'API mxGraph ; courbes d'historique et jauge de ratio (EF-V5) en canvas HTML superposé.
- **Gestion du fichier** : **C2** — l'éditeur écrit (via l'hôte qui reçoit le `save`), le viewer lit. Le modèle ne fait que pousser le chemin en mode auto. *À réévaluer éventuellement.*

## Conséquences

- Palette EF-E1 quasi gratuite, format ouvert et parsable.
- Le viewer doit maîtriser le moteur draw.io + un canvas superposé (R3) ; perf à valider à haute fréquence (100–1000 var × 10–100 Hz).
- Sécurité : le XML reçu n'est jamais injecté via `innerHTML`.
- Deux vues à faire cohabiter (viewer + iframe éditeur dans le même WebView2) — le coût inhérent à A4b, allégé par D30 (une seule fenêtre, aucun port supplémentaire).
