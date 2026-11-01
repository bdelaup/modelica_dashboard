# Options d'architecture — modelica_dashboard

**Projet** : environnement de tableaux de bord interactifs pour OpenModelica (exécution temps réel + supervision externe)
**Version** : 1.0
**Statut** : propositions retenues après tri — **aucune architecture écartée**, aucune préférence retenue ; les choix seront creusés un par un
**Références** : [Spécification de besoins](besoin.md) (EF/ENF), [Options techniques](option_technique.md) (axes G/P/E/C/L/canal/R/F)

---

## 0. Principe de ce document

Ce document formalise les **architectures globales** proposées pour l'environnement, avec leurs avantages, inconvénients et un score sur sept critères. Il fait suite au tri suivant :

- **Écartées** : toute architecture impliquant une **modification d'OpenModelica / OMEdit** (plugin C++/Qt) — hors périmètre décidé.
- **Retenues** : les cinq architectures ci-dessous, à égalité.

**Échelle des scores** : 1 à 5, **5 = très favorable** (pour le déploiement, 5 = très simple à déployer).

---

## 1. Architectures écartées (hors périmètre)

| Option | Raison de l'écart |
|---|---|
| **A1 — Plugin OMEdit complet** | Implique une modification d'OMEdit (plugin C++/Qt) — hors périmètre. |
| **A3 — Hybride OMEdit + web** | Composante plugin OMEdit — hors périmètre. Conséquence : EF-B3 (nom du port) se fait par **paramètre `variableName` explicite** dans le bloc, pas de renommage automatique. |

La **bibliothèque Modelica** (blocs + fonction C externe) reste un livrable du projet : c'est un ajout externe, pas une modification du moteur OpenModelica.

---

## 2. Architectures retenues

### A2 — Web locale « zéro install »

Serveur local écoutant **127.0.0.1 uniquement** (ENF-2), ouvert dans le **navigateur du poste** : éditeur (draw.io embarqué ou custom) + viewer web. Le **pivot vit en thread dans la fonction C externe** côté simulation (P1) ; canal mmap ou sockets loopback (axe canal, à trancher).

- **Avantages** : conforme EF-E2 (aucune installation) et ENF-2 ; stack web standard (HTML/JS + WebSocket loopback), très testable et CI-friendly ; déploiement simple (script ou exe autonome L1/L2) ; navigateur = IHM riche, devtools, responsive ; souplesse pour évolutions (IHM découplée du binaire).
- **Inconvénients** : dépend d'un navigateur moderne présent sur le poste ; ouverture manuelle d'une URL ; priorité plugin de ENF-1 non satisfaite (accepté « à défaut ») ; nom de port saisi manuellement (`variableName`, EF-B3).
- **Exigences concernées** : EF-E2, EF-E3, EF-E4, EF-V1–V6, EF-I1, ENF-1 (à défaut), ENF-2.

### A4a — Binaire unique tout-en-un, IHM en WebView embarqué

Un **seul exécutable** embarque le pivot + un **WebView** servant l'IHM (viewer + mode édition entre deux runs, EF-E4) en loopback ; le modèle s'y connecte en loopback. Sous-options de WebView :

| Sous-option | Plateforme | Poids / dépendance |
|---|---|---|
| **WebView2** (moteur Edge/Chromium) | Windows | Runtime déjà présent sur Windows 10/11, ~zéro poids ajouté ; Windows-only |
| **CEF embarqué** | Windows/Linux/macOS | Binaire lourd (~100–150 Mo embarqués) |
| **Tauri** (WebView natif de l'OS, cœur Rust) | Windows/Linux/macOS | Plus léger qu'Electron, mais WebView de l'OS (comportements hétérogènes) |
| Electron (non privilégié) | cross-platform | ~100 Mo+, dépendance Node embarquée |

- **Avantages** : un seul livrable, zéro install, lancement direct sans navigateur externe ; IHM homogène (viewer + édition dans une fenêtre dédiée) ; conforme EF-E2/EF-E4/ENF-2.
- **Inconvénients** : packaging lourd (CEF/Electron) ou dépendance runtime (WebView2) ou hétérogénéité OS (Tauri) ; tests UI en CI plus difficiles (WebView headless) ; évolutions IHM liées au cycle de release du binaire ; double compétence natif + web.
- **Exigences concernées** : EF-E2, EF-E4, EF-V1–V6, EF-I1, ENF-2.

### A4b — WebView pour le viewer seul + édition externe

Exécutable avec WebView dédié au **viewer temps réel** ; l'édition se fait via un outil externe (draw.io self-hosté en mode embed, ou éditeur custom autonome, axe E1).

- **Avantages** : viewer autonome et ergonomique en temps réel ; édition réutilisable et indépendante ; déploiement encore simple.
- **Inconvénients** : deux IHM à maintenir et à faire cohabiter ; EF-E4 (édition uniquement entre deux runs) plus pénible à garantir ; on paie quand même le coût du WebView embarqué.
- **Exigences concernées** : EF-V1–V6, EF-E4, EF-I1, ENF-2.

### A5 — draw.io-centric

Éditeur = **draw.io self-hosté en mode embed** (`?embed=1&proto=json`, iframe pilotée par `postMessage`) servi en local (build statique embarqué, ou conteneur `jgraph/docker-drawio` bind 127.0.0.1) ; viewer = **GraphViewer** du moteur draw.io mis à jour dynamiquement, widgets temps réel complexes (courbes d'historique, jauge de ratio EF-V5) superposés en canvas (R3) ; pivot = petit serveur local.

- **Avantages** : palette de widgets EF-E1 quasi gratuite (bibliothèques `<mxlibrary>` custom) ; éditeur graphique mature ; format `.drawio` ouvert (XML mxGraphModel) et parsable côté serveur (EF-E3) ; réutilisation maximale d'un projet tiers (Apache 2.0).
- **Inconvénients** : rendu draw.io coûteux à haute fréquence (100–1000 variables à 10–100 Hz) → surcouche canvas nécessaire pour le temps réel ; dépendance à un projet tiers ; déploiement du self-host (build statique embarqué ou docker local) ; tests UI difficiles ; sécurité : ne jamais injecter le XML reçu via `innerHTML`.
- **Exigences concernées** : EF-E1, EF-E2, EF-E3, EF-V5, EF-I1, ENF-2.

### A6 — POC minimal

Pivot en **thread C** (fonction C externe) + **WebSockets loopback** + IHM web custom minimale dans le navigateur du poste. Objectif : valider rapidement le cœur risqué — canal loopback, résolution par nom à l'exécution (EF-I1), flux temps réel, EF-B4/EF-V6 (valeurs par défaut / non-connexion) — avant d'investir dans l'IHM complète.

- **Avantages** : mise en œuvre la plus rapide ; très testable (script léger, CI triviale) ; faible coût d'exploration.
- **Inconvénients** : IHM custom minimale (EF-E1 à reconstruire ensuite) ; ergonomie dashboard faible ; partie à refondre pour la V1.
- **Exigences concernées** : EF-I1, EF-B4, EF-V6, EF-V5 (validation du flux temps réel).

---

## 3. Scores

Scores proposés et **validés par le demandeur** (1–5, 5 = très favorable ; déploiement : 5 = très simple).

| Critère | A2 | A4a | A4b | A5 | A6 |
|---|---|---|---|---|---|
| Complexité de déploiement | 4 | 5 | 4 | 3 | 5 |
| Maintenabilité | 4 | 3 | 3 | 3 | 4 |
| Testabilité (tests auto, CI) | 4 | 3 | 4 | 3 | 5 |
| Performance (suivi du temps réel) | 4 | 4 | 4 | 3 | 4 |
| Ergonomie côté Modelica | 3 | 3 | 3 | 3 | 3 |
| Ergonomie côté dashboard | 4 | 5 | 4 | 4 | 2 |
| Souplesse pour évolutions | 4 | 3 | 3 | 3 | 3 |

**Note** : l'ergonomie côté Modelica est à 3 pour toutes les architectures retenues — conséquence directe de l'exclusion du plugin OMEdit (EF-B3 par paramètre `variableName` manuel).

---

## 4. Axes transversaux restant à creuser

Ces axes de [option_technique.md](option_technique.md) restent ouverts et seront tranchés pour l'architecture (ou les architectures) retenue(s), un par un :

1. **Canal modèle ↔ pivot** : mmap fichier / shm POSIX / sockets 127.0.0.1 / ZeroMQ embarqué (FMI 3.0 noté incompatible EF-I1).
2. **Livrable** (axe L) : L1 script interprété / L2 exe compilé autonome / L3 hybride.
3. **Fichier de définition** (axe C) : C1 pivot uniquement / C2 éditeur écrit + viewer lit ; format JSON / `.drawio` natif / les deux / YAML ; accès du composant Modelica M1 / M2 / M3.
4. **Granularité** (axe G) et **placement du pivot** (axe P) : sous-jacents aux architectures ci-dessus.
5. **Moteur de rendu du viewer** (axe R) : R1 draw.io / R2 web custom / R3 hybride.
6. **Plateformes** : Windows seul, ou + Linux, ou + macOS (impacte le canal et le choix du WebView).

---

## 5. Prochaines étapes

1. Sélectionner l'architecture (ou les architectures) à creuser.
2. Pour chaque choix retenu, trancher les axes transversaux de la section 4, un par un, en s'appuyant sur les exigences du [besoin](besoin.md).
