# Spécification technique — modelica_dashboard (dashboard manager + navigateur)

**Projet** : environnement de tableaux de bord interactifs pour OpenModelica (exécution temps réel + supervision externe)
**Version** : 2.1 (2026-11-04)
**Statut** : la v1.0 (architecture **A4b**, viewer.exe + WebView2 embarqué) est **remplacée par l'[ADR-0015](adr/0015-retour-navigateur-dashboard-manager.md)** (2026-11-04, **acté**) : le **rendu et l'édition se font dans le navigateur du poste**, et le « pivot » s'appelle désormais le **dashboard manager** (processus autonome). Points validés avec le demandeur le 2026-11-04 : port UI fixe `51235`, canal UI **SSE+POST**, livrable **Python embarqué (PyInstaller)**, `shared/` C++ maintenu en parallèle (D45–D47). Document de référence pour l'implémentation.
**Références** : [Spécification de besoins](besoin.md) (EF/ENF), [Options techniques](option_technique.md), [Options d'architecture](options_archtecture.md) (variante de A2 retenue), [ADR](adr/) (décisions actées)

---

## 0. Principe de ce document

Ce document spécifie l'architecture retenue : un **dashboard manager** (processus autonome, développé en Python et packagé en exe) qui sert l'**UI web** (viewer temps réel + éditeur draw.io) au **navigateur du poste** en loopback, et échange avec la simulation via un canal socket dédié. Les blocs — bibliothèque Modelica, dashboard manager, navigateur — sont présentés à l'utilisateur de façon monolithique, mais **cloisonnés en modules** pour la maintenabilité et la testabilité. Toutes les interfaces entre composants sont décrites de façon exhaustive (§8).

**Changements du 2026-11-04 (ADR-0015, acté)** :

- Suppression du moteur de rendu embarqué WebView2 : `viewer/`, `.nuget/`, `dist/`, `build/` (artefacts) et les tests associés sont supprimés ; `shared/` (protocole, parsing `.drawio`, pivot loopback, C++) et `modelica/` sont conservés.
- Le pivot devient le **dashboard manager**, processus autonome (P2 au lieu de P3 embarqué).
- L'UI (viewer + éditeur) est servie par le manager en **HTTP + SSE/POST sur `127.0.0.1:51235`** et ouverte dans le navigateur ; le seul autre port ouvert reste le canal sim `127.0.0.1:51234`.
- Le canal sim ↔ manager et le modèle de messages du protocole sont **inchangés** (version 0 maintenue) ; seul le transport du segment manager ↔ UI change (pont WebView2 → SSE/POST).
- Livrable : script **Python embarqué (PyInstaller)** → exe autonome `dashboard_manager.exe`. Le C++ de `shared/` reste buildé et testé en parallèle (« on essaie de maintenir les deux pour voir »).

Les décisions structurantes sont consignées sous forme d'[ADR](adr/) ; le tableau ci-dessous en est le résumé (les décisions modifiées par l'ADR-0015 sont marquées).

| # | Sujet | Décision retenue | ADR |
|---|---|---|---|
| D1 | Plateformes V1 | Windows uniquement (bibliothèque/sim) ; manager portable, testé sur Linux en CI | [0001](adr/0001-architecture-a4b.md) |
| D2 | Granularité / placement du pivot | **P2 — processus autonome « dashboard manager »** *(modifié ADR-0015 : était P3 embarqué dans le viewer)* | [0015](adr/0015-retour-navigateur-dashboard-manager.md) |
| D3 | Canal modèle ↔ manager | Sockets TCP sur 127.0.0.1, 100–1000 variables à 10–100 Hz | [0003](adr/0003-canal-sockets-loopback.md) |
| D4 | Framing du protocole | JSON texte (newline-delimited) si perfs validées, repli binaire length-prefixed | [0003](adr/0003-canal-sockets-loopback.md) |
| D5 | Adressage sim → manager | Port fixe par défaut sur 127.0.0.1 | [0002](adr/0002-pivot-embarque-port-fixe.md) |
| D6 | Éditeur | draw.io self-hosté, mode embed (`?embed=1&proto=json`), servi par le manager | [0004](adr/0004-editeur-drawio.md) |
| D7 | Format du fichier dashboard | `.drawio` natif (XML mxGraphModel) | [0004](adr/0004-editeur-drawio.md) |
| D8 | Rendu du viewer | R3 hybride : moteur draw.io pour la structure, canvas superposé pour courbes/jauges — dans le navigateur | [0004](adr/0004-editeur-drawio.md) |
| D9 | Chargement de l'IHM | **HTTP + SSE/POST loopback `127.0.0.1:51235` servie par le manager, ouverte dans le navigateur** *(modifié ADR-0015 : était ressources embarquées `app://` dans WebView2)* | [0015](adr/0015-retour-navigateur-dashboard-manager.md) |
| D10 | Lancement / pilotage | L'utilisateur lance le dashboard manager ; la sim est lancée depuis OMEdit ; boutons Run/Pause/Stop côté UI (navigateur) | [0002](adr/0002-pivot-embarque-port-fixe.md) |
| D11 | Chargement du dashboard | Deux modes : auto (le modèle pousse le chemin du fichier en début de sim) ou manuel (chargé par l'utilisateur dans l'UI) | [0002](adr/0002-pivot-embarque-port-fixe.md) |
| D12 | Fichier dashboard — qui écrit/lit | C2 — l'éditeur écrit (via le manager), l'UI lit (via le manager) | [0004](adr/0004-editeur-drawio.md) |
| D13 | Rechargement entre deux runs (EF-E4) | Bouton **Recharger** manuel dans l'UI | [0002](adr/0002-pivot-embarque-port-fixe.md) |
| D14 | Binding par nom (EF-I1) | API runtime du moteur de simulation, résolution à l'exécution | [0006](adr/0006-binding-runtime.md) |
| D15 | Valeur par défaut (EF-B4) | `defaultValue` du bloc si jamais de valeur reçue, sinon dernière valeur reçue conservée | [0006](adr/0006-binding-runtime.md) |
| D16 | Livrable manager | **Exe autonome unique `dashboard_manager.exe` — Python embarqué (PyInstaller), assets web embarqués, sans WebView2** *(modifié ADR-0015, acté 2026-11-04)* | [0015](adr/0015-retour-navigateur-dashboard-manager.md) |
| D17 | Distribution bibliothèque | Package Modelica standard dans les libraries OMEdit + DLL C externe | [0007](adr/0007-livraison.md) |
| D18 | CI | GitHub Actions, jobs Linux (tests) + Windows (DLL + packaging) ; build + tests rejouables en local par le même script | [0008](adr/0008-ci-tests.md) |
| D19 | Tests | Unitaires + intégration + contrat (golden-file) ; logique JS de l'UI en Node ; E2E manager via SSE/POST ; **pas d'E2E navigateur réel en V1** *(modifié ADR-0015)* | [0008](adr/0008-ci-tests.md) |
| D20 | Versioning | SemVer produit + champ `version` dans le fichier et le protocole ; lecture de la version N-1 garantie | [0009](adr/0009-versioning.md) |
| D21 | Langues UI | Français + anglais dès la V1, chaînes externalisées | [0010](adr/0010-i18n-logs.md) |
| D22 | Logs | Fichier log par composant + journal dans l'UI (EF-V6) | [0010](adr/0010-i18n-logs.md) |
| D23 | Sécurité | Loopback (ports 51234 + 51235) + validation `Host` côté UI + taille max — risque accepté, à réévaluer (voir §10) *(modifié ADR-0015)* | [0011](adr/0011-securite.md) |
| D24 | Backpressure | Le manager pousse, le navigateur écrase (seule la valeur la plus récente compte) *(modifié ADR-0015)* | [0012](adr/0012-perf.md) |
| D25 | Multi-instances | Un manager = une simulation (EF-E5 strict) ; une 2ᵉ sim sur le port fixe est refusée | [0002](adr/0002-pivot-embarque-port-fixe.md) |
| D26 | Port fixe du canal | `127.0.0.1:51234` | [0013](adr/0013-decisions-mise-en-oeuvre.md) |
| D27 | OpenModelica cible | v1.27.1 épinglée pour biblio, DLL et CI | [0013](adr/0013-decisions-mise-en-oeuvre.md) |
| D28 | Cœur risqué | POC/spike en parallèle du socle (binding par nom, commandes, temps réel) | [0013](adr/0013-decisions-mise-en-oeuvre.md) |
| D29 | Contrat du canal | `docs/protocole.md` rédigé avant le code ; code et golden files s'y conforment | [0013](adr/0013-decisions-mise-en-oeuvre.md) |
| D30 | Mécanisme de l'éditeur | draw.io en **iframe de la page UI dans le navigateur**, servi par le manager en loopback, piloté par `postMessage` *(remplacé par ADR-0015 : était iframe dans le WebView2)* | [0015](adr/0015-retour-navigateur-dashboard-manager.md) |
| D31 | Versions de départ | Produit v0.1.0, protocole `version: 0`, `dashboardVersion="0"` | [0013](adr/0013-decisions-mise-en-oeuvre.md) |
| D32 | Widgets V1 | `value`, `gauge`, `curve`, `button`, `slider`, `log` (text/image/table en V2) | [0013](adr/0013-decisions-mise-en-oeuvre.md) |
| D33 | Stack | Manager en **Python** (packagé PyInstaller, D47) ; `shared/` **C++20 + CMake** conservé et buildé en parallèle (nlohmann/json, Catch2 v3/CTest) *(modifié ADR-0015)* ; logique JS de l'UI testée en Node LTS | [0013](adr/0013-decisions-mise-en-oeuvre.md) / [0015](adr/0015-retour-navigateur-dashboard-manager.md) |
| D34 | draw.io embarqué | v32.3.0, build statique webapp embarqué dans le **manager** *(modifié ADR-0015 : était `viewer/third_party/`)* | [0013](adr/0013-decisions-mise-en-oeuvre.md) |
| D35 | Taille max de trame | 1 MiB par ligne NDJSON | [0013](adr/0013-decisions-mise-en-oeuvre.md) |
| D36 | Langue UI par défaut | Français (sélecteur FR/EN conservé) | [0013](adr/0013-decisions-mise-en-oeuvre.md) |
| D37 | Logs | `%LOCALAPPDATA%\modelica_dashboard\logs\{manager,sim}.log` *(modifié ADR-0015)* | [0013](adr/0013-decisions-mise-en-oeuvre.md) |
| D38 | DLL C externe | `DashboardExternal.dll`, exports préfixés `dashboard_` | [0013](adr/0013-decisions-mise-en-oeuvre.md) |
| D39 | `showPort` (EF-B2) | Visé : `Placement(visible=showPort)` ; à valider au POC | [0013](adr/0013-decisions-mise-en-oeuvre.md) |
| D40 | État sim→manager | Message `state` sim→manager (alimente `simState`) | [0013](adr/0013-decisions-mise-en-oeuvre.md) |
| D41 | Ordre de dev | protocole.md → socle shared/ + contrat + CI → POC parallèle → manager Python (canal 51234 + HTTP/SSE 51235) → biblio+DLL → UI web navigateur → éditeur *(modifié ADR-0015)* | [0013](adr/0013-decisions-mise-en-oeuvre.md) |
| D42 | Portage Linux (tests) | Cœur C++ portable (shared/) + tests CTest sur Linux ; E2E manager via SSE/POST *(modifié ADR-0015 : remplace viewer_headless)* | [0013](adr/0013-decisions-mise-en-oeuvre.md) |
| D43 | Moteur de rendu / édition | **Navigateur du poste** (suppression du WebView2 embarqué) | [0015](adr/0015-retour-navigateur-dashboard-manager.md) |
| D44 | Nom du composant d'affichage | « pivot » renommé **dashboard manager** | [0015](adr/0015-retour-navigateur-dashboard-manager.md) |
| D45 | Port UI | `127.0.0.1:51235` **fixe** (HTTP statique + SSE/POST) — **acté 2026-11-04** | [0015](adr/0015-retour-navigateur-dashboard-manager.md) |
| D46 | Canal UI navigateur ↔ manager | **SSE** (manager→UI) + **POST** (UI→manager) sur le port UI, messages de `protocole.md` §7 — **acté 2026-11-04** | [0015](adr/0015-retour-navigateur-dashboard-manager.md) |
| D47 | Livraison manager | Script **Python embarqué (PyInstaller)** → exe autonome `dashboard_manager.exe`, assets web embarqués, zéro install — **acté 2026-11-04** ; `shared/` C++ maintenu en parallèle | [0015](adr/0015-retour-navigateur-dashboard-manager.md) |

---

## 1. Architecture globale

```mermaid
flowchart LR
    subgraph OMEdit
        MO[modèle .mo] --> LIB[bibliothèque Modelica<br/>blocs + fonction C externe]
    end
    LIB -->|sockets 127.0.0.1:51234<br/>JSON newline-delimited| DM
    subgraph EXE[processus autonome — dashboard_manager.exe (Python embarqué)]
        DM[dashboard manager<br/>canal sim : serveur 127.0.0.1:51234<br/>UI : serveur HTTP+SSE 127.0.0.1:51235]
        DM --> WEB[assets UI embarqués<br/>viewer + éditeur draw.io self-hosté]
    end
    WEB -->|HTTP + SSE 127.0.0.1:51235| NAV[navigateur du poste<br/>rendu R3 hybride + édition draw.io embed]
    NAV -. save (POST) .-> DM
    DM -->|lit / pousse| F[(fichier .drawio<br/>à côté du .mo)]
```

Flux de données :

1. L'utilisateur lance `dashboard_manager.exe` : le canal sim écoute sur **127.0.0.1:51234** (D26) et l'UI est servie sur **http://127.0.0.1:51235** (D45 fixe, URL imprimée au démarrage ; option `--open` pour lancer le navigateur automatiquement).
2. L'utilisateur ouvre l'URL dans son navigateur (D43) ; la page se connecte en **SSE** (D46).
3. La simulation est lancée depuis OMEdit ; la fonction C externe se connecte au port fixe et s'identifie (handshake, §8.2).
4. En mode **auto**, la sim pousse le chemin du fichier dashboard ; en mode **manuel**, l'utilisateur charge le dashboard dans l'UI.
5. Pendant le run : la sim pousse les valeurs à chaque pas (sim → manager → navigateur, SSE) ; les actionneurs et commandes font le chemin inverse (navigateur → POST → manager → sim).
6. Entre deux runs, l'utilisateur édite le dashboard dans draw.io (servi par le manager) puis clique sur **Recharger** dans l'UI (EF-E4).

---

## 2. Bloc 1 — Bibliothèque Modelica (inchangé)

### 2.1 Contenu

- Package Modelica `Dashboard` (dossier standard, installé dans les libraries OMEdit — D17) contenant :
  - blocs `Dashboard.Input` et `Dashboard.Output` pour **Real, Integer, Boolean** (EF-B1) ;
  - paramètre booléen `showPort` (case à cocher, masquer/afficher le port — EF-B2) ;
  - paramètre `variableName` : nom de la variable du modèle à laquelle le bloc est associé (EF-B3) ;
  - paramètre `defaultValue` (typé selon le bloc) utilisé si la variable n'a jamais reçu de valeur du dashboard (EF-B4, D15) ;
  - paramètre `dashboardFile` (chemin du fichier `.drawio`) utilisé en mode **auto** pour pousser le dashboard au manager en début de simulation (D11).
- Fonction C externe (DLL distribuée avec le package) appelée par les blocs, résolvant les variables **par nom à l'exécution** via l'API runtime du moteur de simulation (D14, EF-I1).

### 2.2 Comportement de la fonction C externe

- **Au démarrage de la simulation** : connexion TCP à `127.0.0.1:51234` (D26) ; handshake (§8.2) incluant la liste des noms de variables du modèle et, en mode auto, le `dashboardFile`.
- **Résolution par nom (EF-I1)** : à chaque appel, la fonction C interroge la table des variables du runtime compilé (nom → adresse/valeur). Aucune recompilation quand le dashboard change.
- **Valeur par défaut (EF-B4, D15)** : si le nom n'a jamais été résolu côté dashboard → `defaultValue` du bloc ; si une valeur a déjà été reçue → dernière valeur reçue conservée.
- **Pendant le run** : à chaque pas de solveur, envoi des sorties des blocs vers le manager (données, §8.3) ; réception et application des actionneurs reçus du manager.
- **Non-connexion** : si un nom du dashboard n'existe pas dans le modèle, la sim ne plante pas ; le manager marque la variable non connectée et la signale à l'UI (EF-V6, §8.4).

### 2.3 Distribution

Dossier `Dashboard/` (fichiers `.mo` + `package.mo` + `Resources/` contenant la DLL C) à copier dans les Modelica libraries d'OMEdit ; chemin de la DLL déclaré dans la configuration OMEdit. Aucun installeur (D17).

---

## 3. Bloc 2 — Dashboard manager (processus autonome, ex-« pivot »)

### 3.1 Rôle

Processus autonome (D2/D44, ADR-0015), développé en **Python** et packagé en exe (PyInstaller, D47) : **double serveur loopback** — canal sim `127.0.0.1:51234` (accepte **une seule** connexion de simulation, D25) + serveur HTTP+SSE `127.0.0.1:51235` pour l'UI navigateur (D45/D46). Il reçoit les sorties du modèle, les distribue à l'UI, reçoit les actionneurs/commandes de l'UI et les retransmet à la sim, lit/pousse le fichier dashboard. Le C++ de `shared/` (protocole, parsing `.drawio`, pivot loopback) est **conservé et buildé en parallèle** (décision 2026-11-04 : « on essaie de maintenir les deux pour voir ») ; les deux implémentations sont validées par les mêmes golden files `tests/contract/`.

### 3.2 Cycle de vie

1. Démarrage du manager → écoute sur `51234` (canal) + sert l'UI sur `51235` (en-tête `Host` validé, §10).
2. Connexion d'une sim → handshake validé → état `CONNECTED`. Toute connexion supplémentaire est **refusée** (log + message), conformément à EF-E5/D25.
3. Réception du dashboard (mode auto poussé par la sim, ou mode manuel demandé dans l'UI) → lecture du fichier `.drawio` → envoi à l'UI + résolution des noms (intersection noms modèle ∩ noms dashboard) :
   - nom présent des deux côtés → flux actif ;
   - nom côté modèle seul → valeur par défaut appliquée (EF-B4) ;
   - nom côté dashboard seul → widget marqué **non connecté** + message dans le journal (EF-V6).
4. Run : relais des données sim → UI à chaque pas ; relais des commandes UI → sim.
5. Déconnexion de la sim → retour à l'état d'écoute (nouvelle sim possible).

### 3.3 Backpressure (D24)

Le manager pousse toutes les valeurs à chaque pas sans jamais bloquer la simulation ; si le navigateur est lent, seules les valeurs les plus récentes sont affichées (les intermédiaires sont écrasés côté navigateur).

---

## 4. Bloc 3 — Navigateur (UI web servie par le manager)

### 4.1 Conteneur

- Le manager sert des **assets statiques** (HTML/JS/CSS + build statique draw.io embarqué, D34) en **HTTP sur `127.0.0.1:51235`** (D9/D45) ; l'utilisateur ouvre `http://127.0.0.1:51235` dans le **navigateur de son choix** (D43). Aucun composant n'est exposé publiquement (ENF-2) ; l'en-tête `Host` est validé côté serveur (§10).
- Échanges manager ↔ navigateur sur le même port (D46) : **SSE** (`text/event-stream`, un `data: {json}\n\n` par message) pour manager → UI, **POST** (body JSON) pour UI → manager — messages JSON structurés de `protocole.md` §7.

### 4.2 Fonctions (EF-V)

- Affichage de la **durée de simulation** en cours (EF-V1).
- Boutons **Run** (temps réel, EF-V2), **Run jusqu'à la fin** (stopTime ou durée saisie, EF-V3), **Pause** (EF-V4) — envoyés au manager en POST comme commandes (§8.5).
- **Jauge de ratio** temps réel/simulé (EF-V5).
- Widgets du dashboard rendus selon R3 (D8) : structure/layout par le moteur draw.io (`viewer-static.min.js` en lecture seule, cellules mises à jour via l'API mxGraph), courbes d'historique et jauge de ratio en **canvas HTML superposé**.
- **Journal de messages** (EF-V6) : variables non connectées, erreurs de protocole, événements de cycle de vie.
- Bouton **Recharger** (D13) : relit le fichier `.drawio` entre deux runs (EF-E4).
- Bouton **Éditer** : ouvre l'éditeur draw.io (iframe dans la page UI, §5) sur le dashboard courant.
- I18n FR/EN (D21) : chaînes externalisées, sélecteur de langue.

### 4.3 Cloisonnement

La logique de l'UI (rendu R3, gestion des widgets, traitement des messages du manager, i18n) est isolée dans une **bibliothèque JS testable en Node** (`manager/web/`) ; le pont SSE/POST est injecté (D19). Cela rend la logique testable sans navigateur.

---

## 5. Bloc 4 — Éditeur (draw.io self-hosté servi par le manager, dans le navigateur)

- **draw.io self-hosté en mode embed** (`?embed=1&proto=json&configure=1`), build statique (**v32.3.0**, D34) embarqué dans le manager et servi en `http://127.0.0.1:51235/…`, chargé dans une **iframe de la page UI ouverte dans le navigateur** (D30 modifié ADR-0015) — aucun port supplémentaire.
- L'iframe est pilotée par `postMessage` : l'UI envoie `load` (XML du dashboard courant), reçoit `save`/`autosave` (XML).
- **Écriture du fichier (C2, D12)** : à chaque `save`, l'UI transmet le XML au manager (POST), qui écrit le fichier **à côté du `.mo`**, sous le nom choisi par l'utilisateur (EF-E3).
- **Palette de widgets (EF-E1, D32)** : bibliothèques de formes custom `<mxlibrary>` servies par le manager, une forme par type de widget V1 (`value`, `gauge`, `curve`, `button`, `slider`, `log`).
- **Sécurité** : le XML reçu n'est **jamais injecté via `innerHTML`** ; il est écrit tel quel sur disque et rendu uniquement via le moteur draw.io ou un parser XML propre.
- L'édition n'est possible qu'entre deux runs (EF-E4) : le bouton Éditer est désactivé pendant un run.

---

## 6. Format du fichier dashboard (`.drawio`)

- Format natif draw.io : `<mxfile><diagram><mxGraphModel><root><mxCell …/></root></mxGraphModel></diagram></mxfile>` (D7).
- Chaque widget = une cellule `UserObject` avec attributs personnalisés :
  - `dashboardVersion` (entier, champ de version du format — D20/D31, **0 en V1**) sur l'élément racine `mxfile` ;
  - `type` : type de widget V1 (`value`, `gauge`, `curve`, `button`, `slider`, `log` — D32 ; `text`, `image`, `table` en V2) ;
  - `variable` : nom de la variable Modelica associée (résolution par nom, EF-I1) ;
  - attributs de style/position natifs mxGraph (`style`, `geometry`, `vertex="1"`…).
- Le fichier est enregistré **à côté du `.mo`**, sous un **nom libre choisi par l'utilisateur** (EF-E3).
- **Versioning (D20)** : un lecteur de version N sait lire les fichiers de version N-1 (champs inconnus ignorés) ; un fichier de version supérieure ou trop ancienne produit un message d'erreur explicite dans le journal. Le même champ `version` figure dans chaque message du protocole (§8).
- **Un seul dashboard actif par modèle** (EF-E5) : l'UI ne charge qu'un fichier à la fois.

---

## 7. Architecture du code (arborescence du dépôt)

Monorepo, sous-dossiers par composant ; `docs/` (Zensical) inchangé à la racine.

```
modelica_dashboard/
├── docs/                  # Zensical : besoin, options, spec_technique, contributeur, protocole, adr/
├── manager/               # dashboard manager (à développer — ADR-0015) : serveur loopback Python,
│   │                      # packagé en exe autonome via PyInstaller (D47)
│   ├── src/               # canal sim 127.0.0.1:51234 + serveur HTTP+SSE/POST UI 127.0.0.1:51235 + routage
│   ├── web/               # UI navigateur : viewer (rendu R3) + éditeur draw.io embed ; logique JS testable en Node
│   └── tests/             # tests pytest du manager + E2E SSE/POST
├── modelica/              # package Modelica Dashboard + fonction C externe (DLL) — inchangé
│   ├── Dashboard/         # .mo (blocs, paramètres)
│   └── csrc/              # fonction C externe, build propre (DLL)
├── shared/                # cœur C++ du contrat, conservé et buildé en parallèle (ADR-0015) :
│   │                      # protocole NDJSON, parsing .drawio, pivot loopback + tests Catch2
│   └── CMakeLists.txt
├── tests/                 # intégration C++ (harnais + end_to_end + bench) + contrat golden-file (Python)
└── scripts/               # build.ps1 / build.sh : rejoue la séquence exacte de la CI en local
```

Règles de cloisonnement :

- `modelica/` et `shared/` (C++) ne se référencent que via `shared/` (contrat) — pas d'include croisé.
- Le manager Python (`manager/`) réimplémente le **même contrat** de canal, validé par les mêmes golden files `tests/contract/` que le C++ de `shared/` (décision ADR-0015 : maintenir les deux en parallèle).
- La logique JS de l'UI (`manager/web/`) n'utilise aucune API navigateur propriétaire : le pont SSE/POST est injecté, ce qui permet de la tester en Node (D19).
- `shared/` et `modelica/csrc/` restent buildables seuls (CMake) ; `scripts/build.ps1` orchestrera aussi le packaging PyInstaller du manager au développement.

---

## 8. Interfaces entre composants (exhaustives)

### 8.1 Canal modèle ↔ manager — transport

- **Transport** : TCP sur `127.0.0.1:51234` (port fixe, D5/D26 ; contrat complet dans [protocole.md](protocole.md)). Une seule connexion acceptée (D25).
- **Framing** : JSON texte **newline-delimited** (un objet par ligne) si les benchmarks à 1000 variables × 100 Hz le permettent (D4) ; repli : trames binaires length-prefixed `[uint32 longueur][uint8 type][payload]`.
- **Validation** : taille maximale par trame **1 MiB** (D35, D23) ; à réévaluer (voir §10).

### 8.2 Handshake (sim → manager, à la connexion)

```json
{"type": "hello", "version": 0, "role": "simulation",
 "variables": ["a.b", "c.d"],
 "dashboardFile": "C:\\models\\Modele.dashboard.drawio"}
```

- `version` : version du protocole (D20/D31, **0 en V1**).
- `variables` : noms (`variableName`) des blocs visibles du modèle.
- `dashboardFile` : présent uniquement en mode **auto** (D11) ; absent en mode manuel.
- Réponse du manager : `{"type": "helloAck", "version": 0, "status": "ok"}` ou `{"type": "error", "code": "…", "message": "…"}`.

### 8.3 Données (sim → manager → navigateur)

Un message par pas de solveur, batchant toutes les variables mises à jour :

```json
{"type": "data", "version": 0, "seq": 1234, "time": 12.34,
 "values": {"a.b": 1.5, "c.d": true}}
```

Le manager ajoute le ratio temps réel/simulé (EF-V5) avant retransmission à l'UI : `{"type": "tick", "version": 0, "seq": 1234, "simTime": 12.34, "ratio": 0.98, "values": {…}}`.

Un message d'état sim→manager complète le flux (D40) : `{"type": "state", "version": 0, "state": "running|paused|stopped|finished"}`.

### 8.4 Signaux d'état (manager → UI, SSE)

- `{"type": "unconnected", "version": 0, "variables": ["x.y"]}` — variables du dashboard absentes du modèle (EF-V6).
- `{"type": "dashboard", "version": 0, "source": "auto|manual", "file": "…", "xml": "<mxfile>…"}` — push du dashboard (mode auto) ou confirmation de chargement (mode manuel).
- `{"type": "simState", "version": 0, "state": "running|paused|stopped|finished"}`.

### 8.5 Commandes (UI → manager, POST → sim)

```json
{"type": "cmd", "version": 0, "cmd": "run"}
{"type": "cmd", "version": 0, "cmd": "pause"}
{"type": "cmd", "version": 0, "cmd": "runToEnd", "duration": 10.0}
{"type": "cmd", "version": 0, "cmd": "stop"}
{"type": "cmd", "version": 0, "cmd": "set", "variable": "a.b", "value": 2.0}
```

`runToEnd` sans `duration` s'arrête au `stopTime` du modèle (EF-V3). Les `set` sont les actionneurs (boutons, sliders) appliqués à la sim.

### 8.6 Interface UI ↔ éditeur (draw.io embed, iframe dans le navigateur — D30 modifié ADR-0015)

- L'éditeur draw.io (build statique embarqué dans le manager, D34) est servi en `http://127.0.0.1:51235/…` et chargé dans une **iframe de la page UI** ouverte dans le navigateur.
- `postMessage` : UI → iframe `{"action": "load", "xml": "…"}` ; iframe → UI `{"event": "save", "xml": "…"}` / `{"event": "autosave", "xml": "…"}`.
- À chaque `save`, l'UI transmet le XML au manager (POST, §8.8) qui écrit le fichier dashboard (C2, D12). L'édition est bloquée pendant un run (EF-E4).

### 8.7 Interface composants ↔ fichier dashboard

- **Éditeur** : écrit le XML (via l'UI puis le manager, §8.6).
- **Manager** : lit le fichier au chargement et au clic sur **Recharger** (D13) ; le pousse à l'UI.
- **Modèle (fonction C)** : en mode auto, pousse seulement le **chemin** du fichier au manager (M2 lecture seule côté modèle) ; ne parse jamais le XML.

### 8.8 Canal navigateur ↔ manager (HTTP + SSE/POST, `127.0.0.1:51235` — D45/D46)

- **HTTP statique** : sert les assets de l'UI (HTML/JS/CSS, build draw.io) et la sonde `GET /api/probe` (remplace les `probe`/`probeAck` du pont WebView2).
- **SSE** (`text/event-stream`, un événement `data: {json}\n\n` par message) : manager → UI — `tick`, `simState`, `unconnected`, `dashboard`, `probeAck` (`protocole.md` §7).
- **POST** (body JSON) : UI → manager — `cmd`, `loadDashboard(path)`, `reload()`, `openEditor()`, `probe`.
- Le transport change (pont WebView2 → SSE/POST, ADR-0015) mais **pas le modèle de messages** : la version du protocole reste **0** (contrat non releasé entre-temps).

---

## 9. Architecture des tests

Pyramide retenue (D19) : **unitaires + intégration + contrat**, pas d'E2E navigateur réel en V1.

| Niveau | Cible | Outil / lieu |
|---|---|---|
| **Unitaires** | Parsing/validation du `.drawio`, protocole (sérialisation, validation de trames) : C++ `shared/` (sockets mockés) **et** manager Python (sockets mockés) ; logique JS de l'UI en Node (pont SSE/POST simulé) | C++ : Catch2 via CTest ; Python : pytest dans `manager/` ; JS : `node --test` dans `manager/web/` |
| **Intégration** | Harnais C + `test_end_to_end` (flux réel sur 51234 : handshake, data, commandes, mode auto/manuel, EF-B4/EF-V6) ; E2E UI via SSE/POST contre le vrai manager (script Python) ; benchmark framing 1000 var × 100 Hz | `tests/integration/` (C++/CMake) + `manager/tests/` (Python), exécutés en local et en CI |
| **Contrat (golden-file)** | Fichier `.drawio` de référence + vecteurs de messages JSON du protocole, comparés à chaque build — valident **les deux** implémentations (C++ shared/ et Python manager) | `tests/contract/` ; toute évolution du format/protocole fait échouer le test volontairement → versionnage explicite (D20) |

- **CI** (D18) : workflow GitHub Actions `build.yml` — job `linux` (build C++ + CTest + contrat + tests Python manager) et job `windows` (idem + packaging DLL + PyInstaller + release sur tag `v*.*.*`). Plus de WebView2/NuGet (ADR-0015).
- **Rejouable en local** : `scripts/build.ps1` (Windows) / `scripts/build.sh` (Linux) exécutent exactement la même séquence que la CI.
- **Non couvert en V1** : E2E sur un navigateur réel (l'UI est testée via sa logique JS isolée, §4.3, et le manager via SSE/POST, §9). À ajouter en V2 si besoin.

---

## 10. Sécurité (ENF-2)

- **Loopback strict** : seuls deux ports TCP sont ouverts, tous deux sur `127.0.0.1` — **51234** (canal sim, D26) et **51235** (UI HTTP+SSE/POST, D45). Aucun composant n'est exposé publiquement (ENF-2).
- **Validation de l'en-tête `Host`** côté serveur UI (`127.0.0.1` / `localhost` uniquement) — mesure anti DNS-rebinding, rendue nécessaire par le navigateur (ADR-0015 ; à reporter dans l'ADR-0011).
- **Validation des entrées** : taille maximale par trame sur le canal : **1 MiB** (D23/D35) ; limite de taille équivalente sur les événements SSE et les body POST.
- **XML** : jamais d'injection via `innerHTML` (§5).
- ⚠️ **Risque accepté, à réévaluer (D23)** : au-delà de la taille max et du `Host`, aucune validation sémantique des trames (pas de vérification d'enum de type, de jeu de caractères des noms, ni d'authentification de la connexion). Tout processus local peut envoyer des données forgées sur les ports fixes. Ce risque est accepté en V1 (menace limitée aux processus de confiance sur le poste) et **doit être réévalué** si le périmètre change (multi-utilisateur, port configurable, exposition réseau). Voir [ADR-0011](adr/0011-securite.md).

---

## 11. Système de livraison

- **Dashboard manager** : script **Python embarqué (PyInstaller)** produisant un seul exe autonome `dashboard_manager.exe` (assets web embarqués — D16/D47). Zéro installation (EF-E2). Distribué en **release GitHub** (artifact du job CI Windows sur tag `v*.*.*`). Le cœur C++ `shared/` reste buildé/testé en parallèle (ADR-0015).
- **Bibliothèque Modelica** : package `Dashboard/` + DLL C externe, distribué en zip à copier dans les libraries OMEdit (D17) ; procédure documentée dans la doc utilisateur.
- **Éditeur** : servi par le manager (build statique draw.io embarqué) — rien à installer séparément (EF-E2).
- **Documentation** : publiée par Zensical sur GitHub Pages (workflow `docs.yml`, ENF-3).
- **Versioning** : SemVer produit ; champ `version` dans le fichier `.drawio` et dans chaque message du protocole ; lecture de la version N-1 garantie (D20).

---

## 12. Documentation

Tout est dans `docs/`, publié par Zensical (ENF-3) :

| Document | Contenu |
|---|---|
| [Spécification de besoins](besoin.md) | EF/ENF validées |
| [Options techniques](option_technique.md) / [Options d'architecture](options_archtecture.md) | Axes et architectures évaluées |
| `spec_technique.md` (ce document) | Architecture retenue (ADR-0015 acté), interfaces, tests, livraison |
| `protocole.md` | Contrat détaillé du canal sim (127.0.0.1:51234, NDJSON, limite 1 MiB) et du canal UI (SSE/POST 127.0.0.1:51235) : handshake, messages, codes d'erreur, cycle de vie, exemples issus des tests de contrat |
| `contributeur.md` | Prérequis (OM v1.27.1, Python 3 + PyInstaller, MSVC/CMake pour shared/, Node LTS — sans WebView2), arborescence, build local (même séquence que la CI), tests, PR |
| `adr/` | Une fiche par décision tranchée (contexte, décision, conséquences), dont [ADR-0015](adr/0015-retour-navigateur-dashboard-manager.md) pour le retour arrière navigateur |
| Doc utilisateur (FR/EN) | Installation du package Modelica, lancement du dashboard manager, édition d'un dashboard, utilisation en run |

---

## 13. Performance et dimensionnement

- Cible : **100–1000 variables à 10–100 Hz** sur le canal loopback (D3).
- Backpressure : le manager pousse, le navigateur écrase (D24) — la simulation n'est jamais bloquée par l'affichage.
- Le framing JSON est validé par un **benchmark** à la cible (1000 var × 100 Hz) en test d'intégration ; si insuffisant, repli sur le framing binaire length-prefixed (D4) sans changer le modèle de messages.
- Le rendu R3 confine les mises à jour fréquentes (valeurs) aux cellules mxGraph et les courbes à un canvas superposé, côté navigateur, pour limiter le coût de redessin du moteur draw.io.

---

## 14. Hors périmètre V1 (rappels)

- Modification du dashboard pendant un run (EF-E4 : entre deux runs uniquement, via le bouton Recharger).
- Linux/macOS du livrable (D1 : Windows uniquement en V1 pour la bibliothèque/sim ; le manager reste portable et testé sur Linux en CI).
- Plusieurs simulations supervisées simultanément (D25 : un manager = une sim).
- E2E sur un navigateur réel en CI (D19).
- Plugin OMEdit (hors périmètre acté, voir [options d'architecture](options_archtecture.md) §1).
