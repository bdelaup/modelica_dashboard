# modelica_dashboard

Environnement de **tableaux de bord interactifs pour OpenModelica** : exécution de modèles en **temps réel** et supervision/action depuis des dashboards externes (grandeurs affichées, boutons interagissant avec la simulation).

## Contenu de cette documentation

- [Spécification de besoins](besoin.md) — exigences fonctionnelles et non fonctionnelles validées, diagramme de cas d'utilisation (SVG).
- [Options techniques](option_technique.md) — axes d'architecture et possibilités ouvertes (aucune préférence retenue).
- [Options d'architecture](options_archtecture.md) — architectures globales retenues après tri (A2, A4a, A4b, A5, A6), avantages/inconvénients et scores.
- [Spécification technique (A4b)](spec_technique.md) — architecture retenue : blocs (bibliothèque, pivot, viewer, éditeur), interfaces exhaustives, format du fichier dashboard, tests, livraison, sécurité, versioning. Les décisions tranchées sont consignées en [ADR](adr/) (dont [ADR-0013](adr/0013-decisions-mise-en-oeuvre.md) pour les décisions de mise en œuvre).
- [Protocole du canal](protocole.md) — contrat détaillé du canal modèle ↔ pivot (127.0.0.1:51234, NDJSON, handshake, messages, codes d'erreur).
- [Guide du contributeur](contributeur.md) — prérequis, arborescence, build local rejouable comme la CI, tests, PR.

## Les trois éléments de l'environnement

1. **Bibliothèque OpenModelica** — blocs d'entrée/sortie (entier, booléen, flottant) avec port masquable/affichable.
2. **Outil de création de dashboards** — éditeur simple, sans installation (plugin OMEdit prioritaire, sinon web local).
3. **Outil d'affichage (viewer)** — Run / Run jusqu'à la fin / Pause, durée de simulation, jauge de ratio temps réel, journal d'informations.
