# Note de POC — binding par nom via l'API runtime (D14/D28)

**Statut** : mécanisme confirmé par lecture du code source OpenModelica **v1.27.1** (ADR-0014) ; compilation/run réels à faire sur un poste avec OM installé (pas disponible dans le sandbox CI Linux du projet — voir ci-dessous).

## 1. Mécanisme (confirmé dans le code source)

```c
// Dans la DLL externe, chaque fonction reçoit threadData (dernier arg implicite omc) :
extern double dashboard_input_real(void* threadData, const char* name, double dflt);

#include "simulation_data.h"   // livré avec OM (OMCompiler/SimulationRuntime/c)
#include "gc/omc_gc.h"         // threadData_t, LOCAL_ROOT_SIMULATION_DATA

DATA *data = (DATA*) ((threadData_t*)threadData)->localRoots[LOCAL_ROOT_SIMULATION_DATA];
// recherche nom -> id (copie de getVarIdByName, modelinfo.c) :
for (int i = 0; i < data->modelData->nVariablesReal; ++i)
  if (strcmp(name, data->modelData->realVarsData[i].info.name) == 0) id = data->modelData->realVarsData[i].info.id;
// lecture/écriture directe de la valeur :
double v = data->realVars[id];   // lecture
data->realVars[id] = newValue;   // écriture (input)
```

Points clés v1.27.1 :
- `threadData_t` + `localRoots[]` : `SimulationRuntime/c/gc/omc_gc.h` (`LOCAL_ROOT_SIMULATION_DATA = LOCAL_ROOT_ERROR_MO`, max 20 roots)
- `DATA`/`MODEL_DATA`/`STATIC_REAL_DATA` : `SimulationRuntime/c/simulation_data.h` (`realVarsData[i].info.name/.id`, `data->realVars`)
- Preuve d'usage : `SimulationRuntime/c/simulation/omc_simulation_util.c:36`
- Logique nom→id : `SimulationRuntime/c/simulation/modelinfo.c:271` (`getVarIdByName`)

## 2. Côté Modelica — déclaration avec threadData

OpenModelica injecte `threadData` comme **dernier argument C** de toute fonction externe. Deux façons de le récupérer côté Modelica (à valider au POC) :

```modelica
function getVar input String name; output Real v;
external "C" v = dashboard_get_real(name)
  annotation (Include="""
#include \"dashboard_client.h\"
extern double dashboard_get_real(threadData_t *threadData, const char* name);
static double dashboard_get_real_impl(const char* name) { /* sans threadData */ }
""", Library="DashboardExternal");
end getVar;
```

En pratique avec omc : déclarer la fonction C **avec** `threadData_t*` en premier, et côté Modelica utiliser la convention `annotation(ExternalObject...)` ou simplement laisser omc injecter — **le POC de compilation tranchera la syntaxe exacte** (issue #16712 montre le pattern `omc_...(threadData_t *threadData, ...)` dans le C généré).

## 3. Pourquoi pas dans le sandbox

- Pas de paquet `omc` dans Debian 13 (apt), pas de binaire Linux autonome récupérable via git seul (les releases officielles passent par build.openmodelica.org / conda-forge `omcompiler` ≤ 1.26.3, et aucun micromamba/conda installable via pip ici).
- Le code source seul ne suffit pas : il faudrait compiler tout OMCompiler (build lourd).
- **Décision** : le POC réel (compile `modelica/Dashboard/Examples/Demo.mo` + run + flux dashboard) se fait sur un poste avec OM v1.27.1 installé, ou en CI via un job qui installe OM (voir `scripts/install-om.sh` à écrire côté CI Windows/Linux avec le paquet officiel).

## 4. Ce qui est déjà validé ici (sans omc)

- Le client C du canal (connexion, hello, data, set, state) : testé de bout en bout contre le vrai dashboard manager (ex-pivot, tests/integration).
- La résolution nom→valeur est simulée par une table interne dans le harness (`DASHBOARD_NO_OMC`) ; en production elle est remplacée par `data->realVars[id]` (ADR-0014).
- La synchro temps réel et le pilotage run/pause restent à valider au POC réel.
