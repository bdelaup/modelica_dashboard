# Options techniques — modelica_dashboard

**Projet** : environnement de tableaux de bord interactifs pour OpenModelica (exécution temps réel + supervision externe)
**Version** : 1.0
**Statut** : en exploration — **aucune préférence retenue**, toutes les possibilités sont listées à égalité
**Référence** : [Spécification de besoins](besoin.md) (EF/ENF)

---

## 0. Principe de ce document

Ce document recense les **options techniques possibles** par axe d'architecture. Aucune option n'est écartée, aucune n'est recommandée : chaque axe présente les possibilités avec leurs avantages, inconvénients et les exigences du besoin concernées. Les décisions seront prises collectivement, en connaissance de cause.

---

## 1. Granularité des composants (axe G)

| Option | Description |
|---|---|
| **G1 — Bloc tout-en-un** | Un seul bloc contient le **pivot + le viewer + l'éditeur** : une application unique avec mode édition (entre deux runs, EF-E4) et mode viewer temps réel, le pivot étant embarqué. |
| **G2 — Viewer + éditeur fusionnés, pivot séparé** | Un outil côté affichage (éditeur intégré au viewer) + un pivot autonome ou côté modèle. |
| **G3 — Trois composants séparés** | Pivot, éditeur autonome et viewer indépendants. |

G1/G2 rendent implicites P3 (pivot embarqué dans le viewer) et E2 (éditeur intégré au viewer).

---

## 2. Placement du pivot (axe P) — composant fit entre OpenModelica et l'affichage

Le **pivot** est le composant d'échange entre le modèle et le dashboard (reçoit les sorties du modèle, distribue vers le viewer, reçoit les actionneurs).

| Option | Description | Conséquence sur le canal |
|---|---|---|
| **P1 — Thread dans la fonction C externe** (côté Modelica) | Le pivot vit dans le processus de simulation, lancé par la bibliothèque | Canal in-process (mémoire interne) ; le viewer se connecte au pivot |
| **P2 — Processus autonome** | Serveur standalone lancé séparément | Canal inter-processus : mmap, sockets 127.0.0.1 ou ZeroMQ (axe 6) |
| **P3 — Embarqué dans le viewer** | Le viewer embarque le serveur pivot | Un seul composant côté affichage ; le modèle se connecte à lui |

---

## 3. Placement de l'éditeur (axe E)

| Option | Description |
|---|---|
| **E1 — Autonome** | Outil d'édition indépendant. Sous-options : draw.io self-hosté (build statique embarqué servi par le serveur local, ou conteneur `jgraph/docker-drawio` en bind 127.0.0.1 avec `DRAWIO_SELF_CONTAINED=1` et `lockdown:true`), ou éditeur custom HTML/Canvas. |
| **E2 — Intégré au viewer** | L'éditeur est un mode/onglet du viewer ; édition uniquement entre deux runs (EF-E4). |

### Notes sur draw.io (diagrams.net, licence Apache 2.0)

- **Mode embed** (`?embed=1&proto=json&configure=1`) : iframe pilotée par `postMessage` ; l'hôte envoie `load` (XML du dashboard), reçoit `save`/`autosave` (XML) — l'éditeur n'a aucun stockage propre, le fichier est écrit à côté du `.mo` par notre application (EF-E3).
- **Format `.drawio`** : XML ouvert et documenté (`<mxfile><diagram><mxGraphModel><root><mxCell …/>`) ; cellules `UserObject` avec attributs personnalisés pour typer les widgets (`type="gauge"`, `variable="…"`). Parsable côté serveur.
- **Palette de widgets** (EF-E1) : bibliothèques de formes custom au format `<mxlibrary>` (XML), servies en local via `clibs` / `defaultCustomLibraries`.
- **Viewer** : `viewer-static.min.js` / `GraphViewer` servi en local rend le `.drawio` en lecture seule ; le moteur permet de mettre à jour des cellules à la volée (labels, styles).
- **Sécurité** : ne jamais injecter le XML reçu via `innerHTML` ; rendre via le viewer ou `<img>` pour les exports SVG.
- draw.io Desktop existe mais entre en tension avec EF-E2 (« aucune installation »).

---

## 4. Gestion du fichier de définition (axe C) — EF-E3

Le dashboard est défini dans un **fichier à côté du `.mo`**, sous un **nom libre** choisi par l'utilisateur.

| Option | Description |
|---|---|
| **C1 — Géré par le pivot uniquement** | Le pivot lit/écrit le fichier ; éditeur et viewer passent par lui. |
| **C2 — Éditeur écrit, viewer lit** | L'éditeur écrit le fichier directement, le viewer le lit. |

**Formats possibles** : JSON avec schéma documenté / `.drawio` natif (XML mxGraphModel) / les deux conservés ensemble (`.drawio` source éditable + JSON dérivé consommé par le viewer) / YAML.

**Accès du composant Modelica au fichier de configuration** :

| Option | Description |
|---|---|
| **M1 — Lecture seule** | Pour contrôles de cohérence (noms de variables modèle vs dashboard, EF-B4/EF-V6) |
| **M2 — Injection** | Le composant Modelica pousse la configuration dans le pivot et/ou le viewer (utile si P1) |
| **M3 — Aucun accès** | Le modèle ne touche jamais au fichier ; résolution par nom à l'exécution uniquement (EF-I1 pur) |

---

## 5. Forme du livrable viewer / pivot / éditeur (axe L)

| Option | Description | Conséquence |
|---|---|---|
| **L1 — Script interprété** | Python + bibliothèques externes (`pyzmq`, `websockets`, `aiohttp`…), lancé par une commande locale | Dépend de l'environnement du poste ; intégration de bibliothèques externes facile ; conforme EF-E2 si l'interpréteur est embarqué |
| **L2 — Exécutable compilé autonome** | Un seul `.exe` (Windows) obtenu par empaquetage (PyInstaller, Nuitka) ou langage compilé (Go, Rust, C++), bind 127.0.0.1 uniquement | Aucune dépendance sur le poste cible ; bibliothèques compilées dedans ; conforme ENF-2 ; livrable plus volumineux |
| **L3 — Hybride** | Viewer en exe compilé, pivot en thread C côté modèle ou en script | Le pivot côté modèle reste du C embarqué dans la simulation |

---

## 6. Canal de communication modèle ↔ pivot (axe canal) — EF-I1, ENF-2

Contrainte de dimensionnement retenue comme cible d'exploration : **100–1000 variables à 10–100 Hz**, résolution par nom à l'exécution (aucune recompilation quand le dashboard change, EF-I1), loopback uniquement (ENF-2).

| Option | Principe | Avantages | Inconvénients |
|---|---|---|---|
| **A — Fichier mappé en mémoire (mmap)** | Fichier binaire à côté du `.mo` mappé par les deux côtés ; table `nom → {valeur, flags}` ; le viewer scrute à fréquence fixe | Portable (Windows/Linux/macOS), débogable (fichier lisible), zéro dépendance externe, local par nature | Pas de push natif ; scrutation périodique |
| **B — Mémoire partagée POSIX** (`shm_open`) | Idem A via segment `/dev/shm` | Légèrement plus rapide, pas de fichier sur disque | Non portable sur Windows sans couche d'émulation |
| **C — Sockets TCP/UDP sur 127.0.0.1** | Datagrammes `nom → valeur` via fonction C externe | Push natif, latence maîtrisée à haute fréquence, extensible (plusieurs clients) | Protocole à définir, ordonnancement moins déterministe |
| **D — ZeroMQ** (`ipc://` ou `tcp://127.0.0.1`) | Bibliothèque de messagerie asynchrone **embarquée** des deux côtés (libzmq côté C, pyzmq côté serveur) ; patterns PUB/SUB, PUSH/PULL, REQ/REP ; broker-less | Pub/sub par nom de variable, rapide, adapté au temps réel, loopback | Dépendance libzmq à embarquer côté modèle et côté serveur |
| **E — FMI 3.0 / co-simulation** | Le viewer pilote le FMU | Standard | **Incompatible EF-I1** (entrées déclarées dans le FMU → recompilation) — noté pour mémoire |

---

## 7. Moteur de rendu du viewer (axe rendu) — EF-V

| Option | Description |
|---|---|
| **R1 — Moteur draw.io (GraphViewer)** | Le viewer réutilise le moteur draw.io servi en local ; cellules du schéma mises à jour dynamiquement (valeurs, styles) à chaque tick ; gère aussi le layout statique. |
| **R2 — Rendu web custom** (HTML / Canvas / SVG) | Viewer indépendant de draw.io ; draw.io ne sert alors que d'éditeur. Widgets temps réel (courbes d'historique, jauge de ratio EF-V5) en canvas/SVG natif. |
| **R3 — Hybride** | Moteur draw.io pour la structure/layout, widgets temps réel complexes (courbes) en canvas superposé. |

---

## 8. Forme des outils (axe forme) — ENF-1, EF-E2

| Option | Description |
|---|---|
| **F1 — Application web servie en local** | Serveur local, écoute 127.0.0.1 uniquement, ouvert dans le navigateur, sans installation (ENF-1 « à défaut », EF-E2) |
| **F2 — Plugin OMEdit** (C++/Qt) | Onglet/fenêtre « Dashboard » natif (priorité ENF-1) ; développement plus lourd, option V2 possible |
| **F3 — Page HTML statique pure** | Sans serveur (File System Access) ; édition peu fluide, pas de viewer temps réel intégré |
| **F4 — Outil existant réutilisé** (Grafana, Node-RED…) | Nécessite installation — **contraire à EF-E2**, noté pour mémoire |

---

## 9. Plateformes cibles (axe plateformes)

- **Windows** (V1)
- **Windows + Linux**
- **Windows + Linux + macOS** (impacte le choix du canal : mmap fichier portable vs mémoire partagée POSIX)

---

## 10. Bibliothèque OpenModelica (EF-B)

- Blocs `Dashboard.Input` / `Dashboard.Output` pour **Real, Integer, Boolean** (EF-B1), bâtis sur une **fonction C externe** résolvant les variables **par nom à l'exécution** (EF-I1, EF-B4 valeur par défaut si absente côté dashboard).
- Paramètre booléen `showPort` = case à cocher masquer/afficher le port (EF-B2).
- Nom du port (EF-B3), deux possibilités : **paramètre `variableName` explicite** dans le bloc, ou **renommage automatique** d'après la variable connectée (nécessite le plugin OMEdit, F2).

---

## 11. Synthèse des axes et combinaisons ouvertes

| Axe | Possibilités |
|---|---|
| G — Granularité | G1 tout-en-un / G2 viewer+éditeur, pivot à part / G3 trois composants séparés |
| P — Pivot | P1 thread fonction C externe / P2 processus autonome / P3 embarqué dans le viewer |
| E — Éditeur | E1 autonome (draw.io build statique embarqué / draw.io docker local / éditeur custom) / E2 intégré au viewer |
| C — Fichier de définition | C1 pivot uniquement / C2 éditeur écrit + viewer lit ; format JSON / `.drawio` natif / les deux / YAML ; accès modèle M1 / M2 / M3 |
| L — Livrable | L1 script interprété + bibliothèques externes / L2 exe compilé autonome / L3 hybride |
| Canal | mmap fichier / shm POSIX / sockets 127.0.0.1 / ZeroMQ (FMI noté pour mémoire, incompatible EF-I1) |
| Rendu viewer | R1 moteur draw.io / R2 rendu web custom / R3 hybride |
| Forme | F1 web locale / F2 plugin OMEdit (option V2) / F3 page statique (F4 noté pour mémoire) |
| Plateformes | Windows V1 / +Linux / +macOS |

Aucune combinaison n'est exclue a priori. Les exigences du besoin (EF-I1, EF-E2, EF-E4, EF-E5, EF-V5, EF-V6, ENF-2) sont rappelées à côté de chaque option concernée.

---

## 12. Points à trancher

1. Granularité G1/G2/G3.
2. Placement du pivot P1/P2/P3.
3. Éditeur autonome (draw.io embarqué / docker local / custom) ou intégré au viewer.
4. Gestion du fichier de définition C1/C2, format, accès modèle M1/M2/M3.
5. Livrable L1/L2/L3.
6. Canal de communication (mmap / sockets / ZeroMQ).
7. Moteur de rendu du viewer R1/R2/R3.
8. Plugin OMEdit en V2 ou non.
9. Plateformes au-delà de Windows.
