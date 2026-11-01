// Parsing/validation du fichier dashboard .drawio (spec §6, D7/D31/D32).
// Parser XML minimal sans dépendance externe : jamais d'injection innerHTML côté viewer.
#pragma once

#include <string>
#include <string_view>
#include <vector>

namespace dashboard {

// D31 — version du format de fichier en V1.
inline constexpr int kDashboardVersion = 0;

// D32 — types de widget V1.
inline constexpr std::string_view kWidgetTypes[] = {
    "value", "gauge", "curve", "button", "slider", "log"
};

struct Widget {
    std::string id;        // id de la mxCell
    std::string type;      // attribut "type" (UserObject)
    std::string variable;  // attribut "variable"
    std::string label;     // attribut "label" (valeur affichée)
    // géométrie mxGraph : x, y, width, height
    double x = 0, y = 0, width = 0, height = 0;
};

struct DashboardFile {
    int dashboardVersion = kDashboardVersion; // attribut mxfile
    std::string name;                          // attribut name du diagram
    std::vector<Widget> widgets;
    bool valid = false;
    std::string error;                         // message d'erreur explicite (journal, D20)
};

// Parse un contenu XML .drawio. Ne lève jamais : valid=false + error si invalide.
// Version > kDashboardVersion ou < kDashboardVersion-1 → erreur explicite (ADR-0009).
DashboardFile parseDashboardFile(std::string_view xml);

// Sérialise un DashboardFile en XML .drawio minimal (utilisé par les tests de contrat
// et comme modèle vierge pour l'éditeur).
std::string serializeDashboardFile(const DashboardFile& dash);

// Intersection noms modèle ∩ noms dashboard (spec §3.3) : retourne les noms du
// dashboard absents du modèle (EF-V6, message "unconnected").
std::vector<std::string> unconnectedVariables(
    const std::vector<std::string>& modelVariables,
    const DashboardFile& dash);

} // namespace dashboard
