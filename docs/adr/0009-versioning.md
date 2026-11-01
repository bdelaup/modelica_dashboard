# ADR-0009 — Versioning : SemVer produit + champ version dans le fichier et le protocole, lecture N-1

**Statut** : acté
**Références** : [spec technique](../spec_technique.md) D20, §6/§11

## Contexte

Le format `.drawio` et le protocole binaire vont évoluer ; il faut une politique de compatibilité explicite pour ne pas casser les dashboards et les modèles existants.

## Décision

- **Produit** : SemVer (MAJOR.MINOR.PATCH), tags `v*.*.*` déclenchent les releases CI.
- **Format** : champ `dashboardVersion` (entier) sur l'élément racine `mxfile` du fichier dashboard.
- **Protocole** : champ `version` dans chaque message du canal (hello, data, tick, cmd…).
- **Politique** : un lecteur de version N sait lire la version **N-1** (champs inconnus ignorés) ; version trop ancienne ou trop récente → message d'erreur explicite dans le journal. Changement cassant = MAJOR.

## Conséquences

- Coût modéré à chaque évolution (une branche de compatibilité), bien moindre que le multi-versions indéfini.
- Les tests de contrat (ADR-0008) matérialisent les versions du format/protocole.
