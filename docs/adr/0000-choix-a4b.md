# ADR-0000 — Choix de l'architecture A4b et merge de la spécification technique

**Statut** : acté (architecture retenue, spécification livrée)
**Date** : 2026-11-01
**Références** : [options d'architecture](../options_archtecture.md) §5 (étape 1 : sélectionner l'architecture), [spécification technique](../spec_technique.md), [PR #3](https://github.com/bdelaup/modelica_dashboard/pull/3)

## Contexte

Cinq architectures globales étaient retenues à égalité après tri (A2, A4a, A4b, A5, A6 — voir [options d'architecture](../options_archtecture.md)). La prochaine étape actée était de sélectionner l'architecture à creuser, puis de trancher les axes transversaux un par un. La spécification technique devait ensuite être rédigée.

## Décision

- **Architecture retenue : A4b** — binaire unique avec WebView2 dédié au viewer temps réel + édition externe (draw.io self-hosté en mode embed).
- **Spécification technique rédigée et livrée** : `docs/spec_technique.md`, accompagnée de 12 ADR (0001–0012) traçant les décisions tranchées (plateformes, pivot, canal, éditeur, rendu, livrable, CI/tests, versioning, i18n/logs, sécurité, perf).
- **Livraison** : PR [#3](https://github.com/bdelaup/modelica_dashboard/pull/3), branche `vibe/spec-technique-a4b-a74da6`, **mergée sur `main`** le 2026-11-01 à 23:09 UTC (commit de merge `63e1fef`, mergée par `bdelaup`).

## Conséquences

- L'architecture A4b est la **référence d'implémentation** ; A2, A4a, A5, A6 restent documentées comme alternatives évaluées mais non retenues.
- Les axes transversaux d'`option_technique.md` sont désormais tranchés pour A4b (voir le tableau D1–D25 de la [spec technique](../spec_technique.md)) ; les ADR 0001–0012 en sont la trace décisionnelle.
- Deux points restent volontairement ouverts et documentés comme tels : le framing JSON du canal (à valider par benchmark, repli binaire prévu — ADR-0003) et la politique de sécurité « loopback + taille max » (risque accepté, à réévaluer — ADR-0011).
- La documentation Zensical est publiée automatiquement sur GitHub Pages par le workflow `docs.yml` à chaque push sur `main`.
