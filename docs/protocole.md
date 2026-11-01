# Protocole du canal modèle ↔ pivot — contrat détaillé

**Projet** : modelica_dashboard (architecture A4b)
**Version du protocole** : **0** (D31, ADR-0013)
**Statut** : contrat de référence — rédigé avant le code (D29) ; les implémentations, golden files et tests s'y conforment. Toute évolution volontaire fait l'objet d'une montée de `version` (politique N-1, ADR-0009).
**Références** : [spec technique](spec_technique.md) §8, [ADR-0003](adr/0003-canal-sockets-loopback.md), [ADR-0013](adr/0013-decisions-mise-en-oeuvre.md)

---

## 1. Transport

- **TCP**, bind/connect sur **`127.0.0.1:51234`** uniquement (D26, ENF-2). Aucune écoute sur une autre interface.
- Le **pivot** (embarqué dans `viewer.exe`) est le **serveur** : il ouvre le port en écoute au démarrage.
- La **simulation** (fonction C externe de la bibliothèque) est le **client** : elle se connecte au démarrage de la sim.
- **Une seule connexion acceptée** (D25) : toute connexion supplémentaire reçoit `error`/`busy` puis est fermée.
- À la déconnexion de la sim, le pivot retourne en écoute (nouvelle sim possible).

## 2. Framing

- **JSON texte newline-delimited (NDJSON)** : un objet JSON par ligne, terminé par `\n` (D4, ADR-0003).
- **Taille maximale d'une ligne : 1 MiB = 1 048 576 octets** (D35, D23). Une ligne plus longue → `error`/`frameTooLarge` puis fermeture de la connexion.
- **Repli binaire** (length-prefixed `[uint32 longueur][uint8 type][payload]`) prévu si le benchmark 1000 var × 100 Hz échoue (ADR-0003) — le modèle de messages ci-dessous est inchangé entre les deux framings.
- Encodage : UTF-8. Un objet invalide (JSON malformé) → `error`/`badJson` puis fermeture.

## 3. Conventions communes

- Chaque message contient **`type`** (string) et **`version`** (int, = 0 en V1).
- **Politique de version** (ADR-0009) : un récepteur de version N lit la version N-1 (champs inconnus ignorés) ; version supérieure ou inférieure à N-1 → `error`/`badVersion`, connexion fermée.
- Les noms de variables sont des **chemins pointés** (`a.b.c`), sensibles à la casse, sans espaces de tête/queue.

## 4. Handshake (sim → pivot, à la connexion)

```json
{"type": "hello", "version": 0, "role": "simulation",
 "variables": ["a.b", "c.d"],
 "dashboardFile": "C:\\models\\Modele.dashboard.drawio"}
```

- `variables` : noms (`variableName`) des blocs **visibles** (`showPort=true`) du modèle.
- `dashboardFile` : présent **uniquement en mode auto** (D11) ; absent en mode manuel.
- Réponse du pivot :

```json
{"type": "helloAck", "version": 0, "status": "ok"}
```

ou, en cas de refus :

```json
{"type": "error", "version": 0, "code": "busy|badVersion|badRole", "message": "..."}
```

puis fermeture de la connexion. Le pivot passe alors à l'état `CONNECTED`.

## 5. Messages sim → pivot

### 5.1 Données (à chaque pas de solveur)

```json
{"type": "data", "version": 0, "seq": 1234, "time": 12.34,
 "values": {"a.b": 1.5, "c.d": true}}
```

- `seq` : compteur croissant, unitaire par connexion. `time` : temps de simulation courant (secondes).
- `values` : batch de toutes les variables mises à jour à ce pas. Types JSON : nombre (Real/Integer), booléen (Boolean), string (texte — V2).

### 5.2 État de la simulation (D40 — ajouté au contrat initial)

```json
{"type": "state", "version": 0, "state": "running|paused|stopped|finished"}
```

Émis par la sim à chaque changement d'état. Le pivot le répercute au viewer sous forme de `simState` (§7.3). `finished` = stopTime atteint (ou durée de `runToEnd` atteinte).

## 6. Messages pivot → sim

### 6.1 Commandes de pilotage

```json
{"type": "cmd", "version": 0, "cmd": "run"}
{"type": "cmd", "version": 0, "cmd": "pause"}
{"type": "cmd", "version": 0, "cmd": "runToEnd", "duration": 10.0}
{"type": "cmd", "version": 0, "cmd": "stop"}
```

- `runToEnd` sans `duration` s'arrête au `stopTime` du modèle (EF-V3).

### 6.2 Actionneurs

```json
{"type": "cmd", "version": 0, "cmd": "set", "variable": "a.b", "value": 2.0}
```

Le pivot accuse réception par `{"type": "ack", "version": 0, "seq": <seq du cmd si présent>}` ; en l'absence d'ack dans un délai raisonnable, le pivot signale l'erreur dans le journal (le délai exact est un détail d'implémentation, pas du contrat).

## 7. Messages pivot → viewer (via le pont WebView2, §8.8 de la spec)

### 7.1 Tick (données + ratio temps réel)

```json
{"type": "tick", "version": 0, "seq": 1234, "simTime": 12.34, "ratio": 0.98, "values": {"a.b": 1.5}}
```

Le pivot calcule `ratio = dt_sim / dt_réel` sur fenêtre glissante et le pousse avec les données (EF-V5).

### 7.2 Variables non connectées (EF-V6)

```json
{"type": "unconnected", "version": 0, "variables": ["x.y"]}
```

Noms présents dans le dashboard mais absents du modèle → widget en état visuel de non-connexion + message dans le journal.

### 7.3 État de la simulation

```json
{"type": "simState", "version": 0, "state": "running|paused|stopped|finished"}
```

Reçu de la sim (§5.2), relayé tel quel.

### 7.4 Dashboard chargé/poussé

```json
{"type": "dashboard", "version": 0, "source": "auto|manual", "file": "...", "xml": "<mxfile>..."}
```

Mode `auto` : poussé par la sim (chemin en §4) puis lu par le pivot. Mode `manual` : chargé par l'utilisateur via `loadDashboard(path)`.

## 8. Codes d'erreur

| `code` | Signification | Action du récepteur |
|---|---|---|
| `badJson` | Ligne non parsable en JSON | ferme la connexion (log) |
| `frameTooLarge` | Ligne > 1 MiB | ferme la connexion (log) |
| `badVersion` | `version` non supportée (hors N / N-1) | ferme la connexion (log + journal) |
| `busy` | une sim est déjà connectée (D25) | ferme la connexion (log + message UI) |
| `badRole` | `role` ≠ `simulation` dans `hello` | ferme la connexion |
| `unknownType` | `type` de message inconnu | ignore le message (log) — champs inconnus ignorés, type inconnu notifié |
| `badCmd` | commande inconnue ou paramètres invalides | ignore + `error` retourné à l'émetteur |

## 9. Cycle de vie type

```
viewer.exe démarre          → pivot écoute 127.0.0.1:51234
sim lancée depuis OMEdit     → connect + hello (variables, [dashboardFile])
                             → helloAck
                             → pivot lit le .drawio, calcule l'intersection des noms
                             → dashboard (xml) + unconnected [...] au viewer
run (bouton viewer)          → cmd run → sim ; state running → simState
boucle                       → data (sim→pivot) → tick (pivot→viewer) à chaque pas
actionneur (bouton/slider)   → cmd set → sim
pause / runToEnd / stop      → cmd … → state → simState
fin (stopTime ou durée)      → state finished → simState finished
sim arrêtée / déconnectée    → retour en écoute
```

## 10. Exemples issus des tests de contrat (golden)

Les vecteurs exacts (fichiers `.ndjson` de référence) vivent dans `tests/contract/`. Exemple minimal :

```
→ {"type":"hello","version":0,"role":"simulation","variables":["motor.speed","motor.on"],"dashboardFile":"C:\\m\\Demo.dashboard.drawio"}
← {"type":"helloAck","version":0,"status":"ok"}
← {"type":"dashboard","version":0,"source":"auto","file":"C:\\m\\Demo.dashboard.drawio","xml":"<mxfile dashboardVersion=\"0\">...</mxfile>"}
← {"type":"unconnected","version":0,"variables":["ghost.var"]}
← {"type":"simState","version":0,"state":"running"}
→ {"type":"data","version":0,"seq":1,"time":0.01,"values":{"motor.speed":0.0,"motor.on":true}}
← {"type":"tick","version":0,"seq":1,"simTime":0.01,"ratio":1.0,"values":{"motor.speed":0.0,"motor.on":true}}
← {"type":"cmd","version":0,"cmd":"set","variable":"motor.on","value":false}
→ {"type":"data","version":0,"seq":2,"time":0.02,"values":{"motor.speed":12.5}}
→ {"type":"state","version":0,"state":"finished"}
← {"type":"simState","version":0,"state":"finished"}
```

`→` = sim vers pivot, `←` = pivot vers sim ou vers viewer.
