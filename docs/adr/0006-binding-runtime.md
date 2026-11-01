# ADR-0006 — Binding par nom via l'API runtime du moteur ; valeur par défaut hybride

**Statut** : acté
**Références** : [spec technique](../spec_technique.md) D14/D15, §2.2 ; EF-I1, EF-B4

## Contexte

EF-I1 : aucune recompilation du modèle quand le dashboard change. La fonction C externe doit retrouver une variable du modèle compilé à partir de son nom, à l'exécution.

## Décision

- **Résolution** : la fonction C externe interroge la **table des variables du runtime compilé** (API runtime du moteur de simulation, mécanisme natif des fonctions externes OpenModelica) : nom → adresse/valeur, lu/écrit directement à chaque appel.
- **Valeur par défaut (EF-B4)** : hybride — si la variable n'a **jamais** reçu de valeur du dashboard → `defaultValue` du bloc (paramètre typé) ; sinon → **dernière valeur reçue conservée**.

## Conséquences

- Le modèle compilé reste inchangé quand on ajoute/retire un widget (EF-I1 pur).
- Un nom absent côté modèle → EF-V6 (widget non connecté + journal), pas de plantage ; un nom absent côté dashboard → EF-B4, pas de plantage.
- Le fichier de mapping généré à la compilation est écarté (dépendance au format du code généré, fragile selon la version d'OpenModelica).
