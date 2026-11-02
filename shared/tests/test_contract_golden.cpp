// Test de contrat cross-language : le parseur C++ (shared/) doit lire le golden
// tests/contract/dashboard_reference.drawio (mêmes règles que test_contract.py).
#include "catch_amalgamated.hpp"
#include "dashboard/dashboard_file.hpp"

#include <cstdlib>
#include <fstream>
#include <sstream>

using namespace dashboard;

static std::string repoRoot() {
    if (const char* r = std::getenv("DASHBOARD_REPO_ROOT")) return r;
    return ".";
}

TEST_CASE("golden .drawio lisible par le parseur C++", "[contract]") {
    std::ifstream f(repoRoot() + "/tests/contract/dashboard_reference.drawio");
    REQUIRE(f.good());
    std::ostringstream ss;
    ss << f.rdbuf();
    auto d = parseDashboardFile(ss.str());
    REQUIRE(d.valid);
    REQUIRE(d.dashboardVersion == 0);
    REQUIRE(d.widgets.size() == 4);
    std::vector<std::string> types;
    for (const auto& w : d.widgets) types.push_back(w.type);
    REQUIRE(types == std::vector<std::string>{"gauge", "button", "curve", "log"});
    REQUIRE(d.widgets[0].variable == "motor.speed");
    REQUIRE(d.widgets[1].variable == "motor.on");
}
