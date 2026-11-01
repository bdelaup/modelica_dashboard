// Contrat C de la DLL externe (D38) — consommé par les blocs Modelica (Blocks.mo).
// La DLL s'appelle DashboardExternal.dll ; tous les exports sont préfixés dashboard_.
//
// Côté POC (D28) : cette API est testée hors OpenModelica via un harness qui joue
// le rôle de la simulation. L'intégration réelle résout la variable par nom via
// l'API runtime du moteur (D14) — point d'extension documenté dans dashboard_client.c.
#ifndef DASHBOARD_CLIENT_H
#define DASHBOARD_CLIENT_H

#ifdef _WIN32
#define DASHBOARD_API __declspec(dllexport)
#else
#define DASHBOARD_API __attribute__((visibility("default")))
#endif

#ifdef __cplusplus
extern "C" {
#endif

// Connexion au pivot (127.0.0.1:51234) + handshake hello avec la liste des
// variables visibles. Appelé au démarrage de la simulation.
// variablesCsv : "a.b,c.d" (noms des blocs showPort=true). dashboardFile : mode auto (D11), peut être "".
// Retourne 0 si helloAck reçu, code d'erreur sinon.
DASHBOARD_API int dashboard_client_connect(const char* variablesCsv, const char* dashboardFile);

// Appelé à chaque pas de solveur par les blocs Output : pousse la valeur vers le pivot.
DASHBOARD_API void dashboard_client_output_real(const char* name, double value, const char* dashboardFile);
DASHBOARD_API void dashboard_client_output_integer(const char* name, int value, const char* dashboardFile);
DASHBOARD_API void dashboard_client_output_boolean(const char* name, int value, const char* dashboardFile);

// Appelé par les blocs Input : retourne la valeur reçue du dashboard, ou defaultValue
// si jamais de valeur reçue (EF-B4, D15 : dernière valeur reçue conservée sinon).
DASHBOARD_API double dashboard_client_input_real(const char* name, double defaultValue, const char* dashboardFile);
DASHBOARD_API int dashboard_client_input_integer(const char* name, int defaultValue, const char* dashboardFile);
DASHBOARD_API int dashboard_client_input_boolean(const char* name, int defaultValue, const char* dashboardFile);

// Signale un changement d'état de la sim (D40) : "running|paused|stopped|finished".
DASHBOARD_API void dashboard_client_state(const char* state);

// Envoie une commande de pilotage (utilisé par les tests ; le pilotage vient du viewer).
DASHBOARD_API int dashboard_client_command(const char* cmdJsonLine);

// Déconnexion propre.
DASHBOARD_API void dashboard_client_disconnect(void);

#ifdef __cplusplus
}
#endif
#endif
