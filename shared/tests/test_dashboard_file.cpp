// Tests du parsing/validation du fichier .drawio (spec §6, D7/D31/D32).
#include "catch_amalgamated.hpp"
#include "dashboard/dashboard_file.hpp"

using namespace dashboard;

static const char* kSample = R"(<?xml version="1.0" encoding="UTF-8"?>
<mxfile dashboardVersion="0" host="modelica_dashboard">
  <diagram name="Demo" id="dashboard">
    <mxGraphModel>
      <root>
        <mxCell id="0"/>
        <mxCell id="1" parent="0"/>
        <mxCell id="w1" type="gauge" variable="motor.speed" label="Vitesse" UserObject="1" vertex="1" parent="1">
          <mxGeometry x="40" y="40" width="120" height="80" as="geometry"/>
        </mxCell>
        <mxCell id="w2" type="button" variable="motor.on" label="Marche" UserObject="1" vertex="1" parent="1">
          <mxGeometry x="200" y="40" width="100" height="40" as="geometry"/>
        </mxCell>
        <mxCell id="w3" type="curve" variable="motor.speed" label="Historique" UserObject="1" vertex="1" parent="1">
          <mxGeometry x="40" y="160" width="300" height="150" as="geometry"/>
        </mxCell>
      </root>
    </mxGraphModel>
  </diagram>
</mxfile>
)";

TEST_CASE("parse un .drawio valide (version 0)", "[dashfile]") {
    auto d = parseDashboardFile(kSample);
    REQUIRE(d.valid);
    REQUIRE(d.dashboardVersion == 0);
    REQUIRE(d.name == "Demo");
    REQUIRE(d.widgets.size() == 3);
    REQUIRE(d.widgets[0].type == "gauge");
    REQUIRE(d.widgets[0].variable == "motor.speed");
    REQUIRE(d.widgets[0].x == 40);
    REQUIRE(d.widgets[0].height == 80);
    REQUIRE(d.widgets[1].type == "button");
}

TEST_CASE("widget de type inconnu -> erreur explicite", "[dashfile]") {
    std::string xml = kSample;
    auto pos = xml.find("type=\"gauge\"");
    xml.replace(pos, 12, "type=\"hologram\"");
    auto d = parseDashboardFile(xml);
    REQUIRE_FALSE(d.valid);
    REQUIRE(d.error.find("hologram") != std::string::npos);
}

TEST_CASE("version de fichier trop récente ou trop ancienne -> erreur (ADR-0009)", "[dashfile]") {
    std::string xml = kSample;
    auto pos = xml.find("dashboardVersion=\"0\"");
    xml.replace(pos, 20, "dashboardVersion=\"5\"");
    auto d = parseDashboardFile(xml);
    REQUIRE_FALSE(d.valid);
    REQUIRE(d.error.find("dashboardVersion") != std::string::npos);
}

TEST_CASE("XML malformé -> erreur, jamais d'exception", "[dashfile]") {
    auto d = parseDashboardFile("<mxfile><diagram>");
    REQUIRE_FALSE(d.valid);
    REQUIRE_FALSE(d.error.empty());
    auto d2 = parseDashboardFile("pas du xml");
    REQUIRE_FALSE(d2.valid);
}

TEST_CASE("sérialise puis reparse (round-trip)", "[dashfile]") {
    DashboardFile in;
    in.dashboardVersion = 0;
    in.name = "RT";
    in.widgets.push_back({"w1", "slider", "motor.throttle", "Gaz", 10, 10, 200, 30});
    in.widgets.push_back({"w2", "log", "", "Journal", 10, 60, 400, 200});
    auto xml = serializeDashboardFile(in);
    auto out = parseDashboardFile(xml);
    REQUIRE(out.valid);
    REQUIRE(out.widgets.size() == 2);
    REQUIRE(out.widgets[0].type == "slider");
    REQUIRE(out.widgets[0].variable == "motor.throttle");
    REQUIRE(out.widgets[1].type == "log");
}

TEST_CASE("unconnectedVariables : noms dashboard absents du modèle (EF-V6)", "[dashfile]") {
    auto d = parseDashboardFile(kSample);
    REQUIRE(d.valid);
    auto unc = unconnectedVariables({"motor.speed", "other"}, d);
    REQUIRE(unc == std::vector<std::string>{"motor.on"});
    auto unc2 = unconnectedVariables({"motor.speed", "motor.on"}, d);
    REQUIRE(unc2.empty());
}

TEST_CASE("les 6 types de widget V1 sont acceptés (D32)", "[dashfile]") {
    for (auto t : {"value", "gauge", "curve", "button", "slider", "log"}) {
        std::string xml = std::string("<mxfile dashboardVersion=\"0\"><diagram name=\"x\"><mxGraphModel><root>"
            "<mxCell id=\"0\"/><mxCell id=\"1\" parent=\"0\"/>"
            "<mxCell id=\"w\" type=\"") + t + "\" variable=\"v\" UserObject=\"1\" vertex=\"1\" parent=\"1\">"
            "<mxGeometry x=\"0\" y=\"0\" width=\"10\" height=\"10\" as=\"geometry\"/></mxCell>"
            "</root></mxGraphModel></diagram></mxfile>";
        auto d = parseDashboardFile(xml);
        REQUIRE(d.valid);
        REQUIRE(d.widgets.size() == 1);
    }
}
