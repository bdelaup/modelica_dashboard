# ADR-0002 — Port fixe par défaut, lancement viewer/sim, bouton Recharger, une sim par viewer

**Statut** : acté
**Références** : [spec technique](../spec_technique.md) D5/D10/D11/D13/D25

## Contexte

Avec le pivot embarqué (ADR-0001), la simulation lancée depuis OMEdit doit retrouver le viewer sur 127.0.0.1. EF-E4 impose l'édition entre deux runs uniquement. EF-E5 impose un seul dashboard/viewer actif par modèle.

## Décision

- **Adressage** : le pivot écoute sur un **port fixe par défaut** sur 127.0.0.1 ; la fonction C externe s'y connecte au démarrage de la sim. (Le port dynamique annoncé via fichier à côté du `.mo` est écarté en V1.)
- **Lancement** : l'utilisateur lance le **viewer** ; la simulation est déclenchée côté **OMEdit** ; les boutons Run / Run jusqu'à la fin / Pause sont côté **viewer** et transitent par le canal vers la sim.
- **Chargement du dashboard** : deux modes — **auto** (le modèle pousse le chemin du fichier dashboard en début de simulation, via le paramètre `dashboardFile` du bloc) et **manuel** (l'utilisateur charge le dashboard dans le viewer).
- **Rechargement (EF-E4)** : **bouton Recharger manuel** dans le viewer entre deux runs (pas de file-watch automatique).
- **Multi-instances** : **un viewer = une simulation** (EF-E5 strict) ; une 2ᵉ connexion sim sur le port fixe est refusée par le pivot (log + message).

## Conséquences

- Un seul port fixe à documenter et à surveiller (ENF-2).
- Deux viewers simultanés entrent en conflit sur le port fixe — accepté en V1 (hors EF-E5).
- Le bouton Recharger évite un watcher de fichiers, au prix d'un clic manuel oubliable.
- Le canal est bidirectionnel : données sim→viewer et commandes/actionneurs viewer→sim.
