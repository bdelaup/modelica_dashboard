# ADR-0008 — CI GitHub Actions (job Windows) + tests unitaires/intégration/contrat, rejouables en local

**Statut** : acté
**Références** : [spec technique](../spec_technique.md) D18/D19, §9

## Contexte

Besoin d'un build reproductible et de tests automatisés pour exe C++, DLL C et logique JS, sur Windows (ADR-0001).

## Décision

- **CI** : workflow GitHub Actions `build.yml` sur runner `windows-latest` (WebView2 présent nativement) : build de chaque composant, tests unitaires (CTest), tests d'intégration (modèle de démo compilé par `omc`, flux réel sur le port fixe), tests de contrat (golden-file), packaging, publication en artifact ; release GitHub sur tag `v*.*.*`.
- **Rejouable en local** : `scripts/build.ps1` exécute exactement la même séquence que la CI.
- **Pyramide de tests** : **unitaires + intégration + contrat**, logique JS du viewer testée en Node (pont WebView2 simulé) ; **pas d'E2E WebView2 réel en V1** (à ajouter en V2 si besoin).

## Conséquences

- Livrable reproductible en CI ; le développeur rejoue localement par un seul script.
- Tests de contrat : toute évolution du format `.drawio` ou du protocole fait échouer le build volontairement → versionnage explicite (ADR-0009).
- Le WebView2 réel n'est pas couvert en CI (coût/fragilité) : la logique du viewer est isolée dans une bibliothèque JS testable sans navigateur.
- Le CI "léger" (tests portables + docs seuls) est écarté : le livrable ne serait pas reproductible.
