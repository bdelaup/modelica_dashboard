# ADR-0003 — Canal modèle ↔ pivot : sockets TCP 127.0.0.1, framing JSON (repli binaire)

**Statut** : acté, framing à valider par benchmark
**Références** : [options techniques](../option_technique.md) §6, [spec technique](../spec_technique.md) D3/D4, §8/§13

## Contexte

Cible de dimensionnement : 100–1000 variables à 10–100 Hz, loopback uniquement (ENF-2), résolution par nom à l'exécution (EF-I1). Options : mmap fichier, shm POSIX (non portable Windows), sockets 127.0.0.1, ZeroMQ embarqué (dépendance libzmq des deux côtés), FMI 3.0 (incompatible EF-I1, écarté).

## Décision

- **Transport** : **sockets TCP sur 127.0.0.1** (port fixe, ADR-0002) — zéro dépendance externe, push natif, latence maîtrisée à 100 Hz.
- **Framing** : **JSON texte newline-delimited** (un objet par ligne) si le benchmark à 1000 var × 100 Hz le permet ; **repli** : trames binaires length-prefixed `[uint32 longueur][uint8 type][payload]`. Le modèle de messages (hello, data, tick, cmd…) ne change pas entre les deux framings.

## Conséquences

- Protocole lisible en debug (netcat), parsing JSON côté C à chaque trame — d'où le benchmark obligatoire en test d'intégration.
- ZeroMQ écarté : embarquer libzmq côté modèle (C) est un coût inutile face à des sockets natifs.
- mmap écarté : pas de push natif, scrutation périodique moins adaptée au temps réel.
