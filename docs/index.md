# modelica_dashboard

Environnement de **tableaux de bord interactifs pour OpenModelica** : exécution de modèles en **temps réel** et supervision/action depuis des dashboards externes (grandeurs affichées, boutons interagissant avec la simulation).

## Contenu de cette documentation

- [Spécification de besoins](besoin.md) — exigences fonctionnelles et non fonctionnelles validées, diagramme de cas d'utilisation (SVG).
- [Options techniques](option_technique.md) — axes d'architecture et possibilités ouvertes (aucune préférence retenue).

## Les trois éléments de l'environnement

1. **Bibliothèque OpenModelica** — blocs d'entrée/sortie (entier, booléen, flottant) avec port masquable/affichable.
2. **Outil de création de dashboards** — éditeur simple, sans installation (plugin OMEdit prioritaire, sinon web local).
3. **Outil d'affichage (viewer)** — Run / Run jusqu'à la fin / Pause, durée de simulation, jauge de ratio temps réel, journal d'informations.
