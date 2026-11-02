within Dashboard;
package Blocks
  "Blocs d'entrée/sortie dashboard (EF-B1)."

  partial block Interface
    "Interface commune : paramètres showPort / variableName / defaultValue / dashboardFile."
    parameter Boolean showPort = true "Masque ou affiche le port (EF-B2)";
    parameter String variableName = "" "Nom de la variable du modèle associée (EF-B3)";
    parameter String dashboardFile = "" "Fichier .drawio poussé au pivot en mode auto (D11)";
  end Interface;

  package Input
    "Blocs d'entrée : le dashboard pilote la variable du modèle (actionneurs)."

    block Real
      extends Interface;
      parameter Real defaultValue = 0.0 "Valeur si jamais de valeur reçue du dashboard (EF-B4)";
      Real y "Sortie du bloc = valeur appliquée au modèle";
      external "C" y = dashboard_input_real(variableName, defaultValue, dashboardFile)
        annotation (Include="""
#include "dashboard_client.h"
static double dashboard_input_real(const char* name, double dflt, const char* dashFile) {
  return dashboard_client_input_real(name, dflt, dashFile);
}
""", Library="DashboardExternal");
    end Real;

    block Integer
      extends Interface;
      parameter Integer defaultValue = 0;
      Integer y;
      external "C" y = dashboard_input_integer(variableName, defaultValue, dashboardFile)
        annotation (Include="""
#include "dashboard_client.h"
static int dashboard_input_integer(const char* name, int dflt, const char* dashFile) {
  return dashboard_client_input_integer(name, dflt, dashFile);
}
""", Library="DashboardExternal");
    end Integer;

    block Boolean
      extends Interface;
      parameter Boolean defaultValue = false;
      Boolean y;
      external "C" y = dashboard_input_boolean(variableName, defaultValue, dashboardFile)
        annotation (Include="""
#include "dashboard_client.h"
static int dashboard_input_boolean(const char* name, int dflt, const char* dashFile) {
  return dashboard_client_input_boolean(name, dflt, dashFile);
}
""", Library="DashboardExternal");
    end Boolean;
  end Input;

  package Output
    "Blocs de sortie : la variable du modèle est poussée vers le dashboard."

    block Real
      extends Interface;
      input Real u "Entrée du bloc = variable du modèle à afficher";
      external "C" dashboard_output_real(variableName, u, dashboardFile)
        annotation (Include="""
#include "dashboard_client.h"
static void dashboard_output_real(const char* name, double v, const char* dashFile) {
  dashboard_client_output_real(name, v, dashFile);
}
""", Library="DashboardExternal");
    end Real;

    block Integer
      extends Interface;
      input Integer u;
      external "C" dashboard_output_integer(variableName, u, dashboardFile)
        annotation (Include="""
#include "dashboard_client.h"
static void dashboard_output_integer(const char* name, int v, const char* dashFile) {
  dashboard_client_output_integer(name, v, dashFile);
}
""", Library="DashboardExternal");
    end Integer;

    block Boolean
      extends Interface;
      input Boolean u;
      external "C" dashboard_output_boolean(variableName, u, dashboardFile)
        annotation (Include="""
#include "dashboard_client.h"
static void dashboard_output_boolean(const char* name, int v, const char* dashFile) {
  dashboard_client_output_boolean(name, v, dashFile);
}
""", Library="DashboardExternal");
    end Boolean;
  end Output;
end Blocks;
