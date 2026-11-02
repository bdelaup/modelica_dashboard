// Harness de test : joue le rôle de la simulation sans OpenModelica.
// Exporte harness_run() (appelé in-process par le test d'intégration) et garde
// un main() CLI pour un lancement manuel.
#include "dashboard_client.h"
#include <stdio.h>
#include <string.h>
#include <stdlib.h>

#if defined(_WIN32)
#include <windows.h>
static void sleep_ms(int ms) { Sleep(ms); }
#else
#include <unistd.h>
static void sleep_ms(int ms) { usleep(ms * 1000); }
#endif

// Joue la sim : connect, 3 outputs, attend delaiInputMs (le test envoie un set
// entre-temps), lit l'input (EF-B4), state running, déconnecte.
// Retourne la valeur lue pour motor.throttle via *outThrottle.
int harness_run(const char* variablesCsv, const char* dashFile, int delaiInputMs, double* outThrottle) {
    int rc = dashboard_client_connect(variablesCsv, dashFile);
    if (rc != 0) { fprintf(stderr, "connect rc=%d\n", rc); return 1; }
    dashboard_client_output_real("motor.speed", 12.5, dashFile);
    dashboard_client_output_boolean("motor.on", 1, dashFile);
    dashboard_client_output_integer("motor.gear", 3, dashFile);
    if (delaiInputMs > 0) sleep_ms(delaiInputMs);
    double v = dashboard_client_input_real("motor.throttle", 0.25, dashFile);
    if (outThrottle) *outThrottle = v;
    dashboard_client_state("running");
    dashboard_client_disconnect();
    return 0;
}

#ifndef DASHBOARD_HARNESS_NO_MAIN
int main(int argc, char** argv) {
    if (argc < 2) { fprintf(stderr, "usage: harness <variablesCsv> [dashboardFile] [delaiInputMs]\n"); return 2; }
    const char* dashFile = argc > 2 ? argv[2] : "";
    int delai = argc > 3 ? atoi(argv[3]) : 0;
    double v = 0;
    int rc = harness_run(argv[1], dashFile, delai, &v);
    printf("input motor.throttle = %g\n", v);
    return rc;
}
#endif
