# modelica_dashboard

Environnement de **tableaux de bord interactifs pour OpenModelica** : exécution de modèles en **temps réel** et supervision/action depuis des dashboards externes (grandeurs affichées, boutons interagissant avec la simulation).

Depuis le 2026-11-04 ([ADR-0015](adr/0015-retour-navigateur-dashboard-manager.md)), le **rendu et l'édition se font dans le navigateur du poste** : un processus autonome, le **dashboard manager** (ex-« pivot »), sert l'UI web en local (loopback) et échange avec la simulation sur `127.0.0.1:51234`.

## Contenu de cette documentation

- [Spécification de besoins](besoin.md) — exigences fonctionnelles et non fonctionnelles validées, diagramme de cas d'utilisation (SVG).
- [Options techniques](option_technique.md) — axes d'architecture et possibilités ouvertes (document d'exploration historique).
- [Options d'architecture](options_archtecture.md) — architectures globales évaluées (A2, A4a, A4b, A5, A6) ; variante de **A2 retenue** (ADR-0015).
- [Spécification technique](spec_technique.md) — architecture retenue : blocs (bibliothèque, dashboard manager, navigateur), interfaces exhaustives, format du fichier dashboard, tests, livraison, sécurité, versioning. Les décisions tranchées sont consignées en [ADR](adr/) (dont [ADR-0015](adr/0015-retour-navigateur-dashboard-manager.md) pour le retour arrière navigateur).
- [Protocole du canal](protocole.md) — contrat détaillé du canal sim ↔ manager (127.0.0.1:51234, NDJSON, handshake, messages, codes d'erreur) et du canal UI manager ↔ navigateur (SSE/POST 127.0.0.1:51235).
- [Guide du contributeur](contributeur.md) — prérequis, arborescence, build local rejouable comme la CI, tests, PR.
- [Note de POC binding](poc-binding-runtime.md) — résolution par nom via l'API runtime OM.
- [ADR](adr/index.md) — une fiche par décision tranchée.

## Les trois éléments de l'environnement

1. **Bibliothèque OpenModelica** — blocs d'entrée/sortie (entier, booléen, flottant) avec port masquable/affichable.
2. **Outil de création de dashboards** — éditeur draw.io self-hosté servi en local par le dashboard manager, ouvert dans le navigateur (aucune installation).
3. **Outil d'affichage (viewer)** — UI web servie par le dashboard manager et ouverte dans le navigateur : Run / Run jusqu'à la fin / Pause, durée de simulation, jauge de ratio temps réel, journal d'informations.
