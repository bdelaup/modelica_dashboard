# ADR-0014 — Détail d'implémentation du binding par nom (D14) : accès via `threadData` du runtime OM

**Statut** : acté (étude du code source OpenModelica — mécanisme **identique en v1.24.0, v1.26.4 et v1.27.1**)
**Références** : [ADR-0006](0006-binding-runtime.md) (D14/D15), [spec technique](../spec_technique.md) §2.2, EF-I1, [POC](../poc-binding-runtime.md)

## Contexte

EF-I1 exige qu'aucune recompilation du modèle ne soit nécessaire quand le dashboard change : la fonction C externe doit retrouver une variable du modèle compilé **par son nom, à l'exécution**. Le POC D28 devait valider le mécanisme exact dans le runtime OpenModelica.

## Mécanisme confirmé dans le code source v1.27.1

1. **Signature implicite** : toute fonction `external "C"` d'OpenModelica reçoit un dernier argument caché `threadData_t *threadData` (généré par omc dans le C produit — confirmé dans `SimulationRuntime` et les issues récentes, ex. #16712 : `omc_...(threadData_t *threadData, ...)`).
2. **Accès aux données du modèle** : `DATA *data = (DATA*) threadData->localRoots[LOCAL_ROOT_SIMULATION_DATA];` — confirmé dans `SimulationRuntime/c/simulation/omc_simulation_util.c:36` (`LOCAL_ROOT_SIMULATION_DATA` = `LOCAL_ROOT_ERROR_MO`, défini dans `SimulationRuntime/c/gc/omc_gc.h:113`).
3. **Table nom → id** : `data->modelData->realVarsData[i].info.name` / `.info.id` (idem `integerVarsData`, `booleanVarsData`, `stringVarsData`, et paramètres `realParameterData`…), `nVariablesReal` etc. — `SimulationRuntime/c/simulation/modelinfo.c:271` (`getVarIdByName`, logique copiable) et `simulation_data.h` (`STATIC_REAL_DATA { VAR_INFO info; ... }`, `DATA { MODEL_DATA* modelData; ... }`).
4. **Valeur courante** : `data->realVars[id]` (tableau `modelica_real* realVars` de `DATA`), idem `integerVars`, `booleanVars`. L'id est un index direct dans ces tableaux.

## Décision

- Les blocs `Dashboard.*` passent `threadData` en dernier argument de la fonction C externe (Modelica : `input` caché impossible → on déclare la fonction avec un paramètre supplémentaire de type `ExternalObject` ou on utilise la convention omc qui injecte `threadData` automatiquement ; **à valider à la compilation du POC**, repli : déclarer explicitement `external "C" f(..., threadData) annotation(..., __OpenModelica_...)` — voir note POC).
- `dashboard_client.c` résout `nom → id` une fois à la première utilisation via la table `modelData` (recherche linéaire comme `getVarIdByName`, mise en cache), puis lit/écrit directement `data->realVars[id]` à chaque appel.
- Le harness de test (hors OM) garde l'ancienne table interne via une macro `DASHBOARD_NO_OMC`.

## Conséquences

- Aucune dépendance au format du code généré (ADR-0006) : on passe par les structures runtime stables (`simulation_data.h`, `omc_gc.h`) livrées avec OM.
- Fragilité résiduelle : ces headers sont internes au runtime (pas une API publique formelle) — acceptable en V1 avec OM épinglé v1.27.1 (D27) ; à réévaluer si bump de version OM.
- Le POC réel (compilation + run avec omc) reste à faire sur poste Windows/Linux avec OM installé.
- **Compatibilité** : le mécanisme est identique en v1.24.0 / v1.26.4 / v1.27.1 (mêmes définitions dans `omc_gc.h`, `simulation_data.h`, `modelinfo.c`, `omc_simulation_util.c`) → le code fonctionnera aussi sur **1.24.x** ; on garde v1.27.1 épinglé (D27) pour le dev/CI, mais 1.24.x est supporté de fait.
