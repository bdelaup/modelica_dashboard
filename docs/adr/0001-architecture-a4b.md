# ADR-0001 — Architecture A4b retenue, Windows-only en V1, pivot embarqué

**Statut** : acté
**Références** : [options d'architecture](../options_archtecture.md) (A4b), [spec technique](../spec_technique.md) D1/D2

## Contexte

Cinq architectures retenues à égalité (A2, A4a, A4b, A5, A6). A4b = WebView pour le viewer seul + édition externe. La plateforme cible conditionne le choix du WebView et la portabilité du canal.

## Décision

- **Architecture A4b** : binaire unique avec WebView2 dédié au viewer temps réel ; édition via outil externe (draw.io).
- **Windows uniquement en V1** : WebView2 est le runtime Edge/Chromium déjà présent sur Windows 10/11 → zéro installation (EF-E2). Linux/macOS en V2+.
- **P3 — le pivot est embarqué dans le binaire viewer** (un seul livrable exe) plutôt que thread C dans la sim (P1) ou processus autonome (P2).

## Conséquences

- Un seul composant à lancer et à distribuer côté affichage.
- Le pivot meurt avec le viewer ; tests headless facilités.
- Le canal modèle ↔ pivot est nécessairement inter-processus (loopback), voir ADR-0003.
- Aucune contrainte de portabilité du canal en V1 (shm POSIX exclu d'office).
