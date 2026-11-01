// Harness de test : joue le rôle de la simulation sans OpenModelica.
// Construit une mini "DLL" (ou exe de test) qui appelle dashboard_client_*
// contre un vrai pivot (shared/) sur 127.0.0.1:51234.
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

int main(int argc, char** argv) {
    if (argc < 2) { fprintf(stderr, "usage: harness <variablesCsv> [dashboardFile] [delaiInputMs]\n"); return 2; }
    const char* dashFile = argc > 2 ? argv[2] : "";
    int delai = argc > 3 ? atoi(argv[3]) : 0;
    int rc = dashboard_client_connect(argv[1], dashFile);
    if (rc != 0) { fprintf(stderr, "connect rc=%d\n", rc); return 1; }
    printf("connected\n");
    fflush(stdout);
    dashboard_client_output_real("motor.speed", 12.5, dashFile);
    dashboard_client_output_boolean("motor.on", 1, dashFile);
    dashboard_client_output_integer("motor.gear", 3, dashFile);
    if (delai > 0) sleep_ms(delai); /* laisse le test envoyer un cmd set entre-temps */
    double v = dashboard_client_input_real("motor.throttle", 0.25, dashFile);
    printf("input motor.throttle = %g\n", v);
    fflush(stdout);
    dashboard_client_state("running");
    dashboard_client_disconnect();
    printf("done\n");
    return 0;
}
