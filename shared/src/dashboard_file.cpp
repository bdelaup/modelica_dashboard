#include "dashboard/dashboard_file.hpp"

#include <algorithm>
#include <cctype>
#include <sstream>

namespace dashboard {

namespace {

// --- Mini parseur XML (lecture seule, sans dépendance) ---
// Supporte : éléments, attributs double-quote, texte, commentaires, déclaration.
struct XmlAttr { std::string name; std::string value; };
struct XmlNode {
    std::string name;
    std::vector<XmlAttr> attrs;
    std::vector<XmlNode> children;
    std::string text;
};

std::string decodeEntities(std::string s) {
    const std::pair<std::string, std::string> ents[] = {
        {"&lt;", "<"}, {"&gt;", ">"}, {"&quot;", "\""}, {"&apos;", "'"}, {"&amp;", "&"}
    };
    for (const auto& [e, r] : ents) {
        std::size_t p = 0;
        while ((p = s.find(e, p)) != std::string::npos) { s.replace(p, e.size(), r); p += r.size(); }
    }
    return s;
}

std::string_view trim(std::string_view s) {
    while (!s.empty() && std::isspace(static_cast<unsigned char>(s.front()))) s.remove_prefix(1);
    while (!s.empty() && std::isspace(static_cast<unsigned char>(s.back()))) s.remove_suffix(1);
    return s;
}

struct Parser {
    std::string_view s;
    std::size_t i = 0;
    bool ok = true;

    void skipWs() { while (i < s.size() && std::isspace(static_cast<unsigned char>(s[i]))) ++i; }
    bool startsWith(std::string_view p) const { return s.substr(i, p.size()) == p; }

    bool parseNode(XmlNode& out) {
        skipWs();
        if (!startsWith("<") || startsWith("</") || startsWith("<?") || startsWith("<!--") || startsWith("<!")) {
            if (startsWith("<?")) { // déclaration / processing instruction
                auto e = s.find("?>", i);
                if (e == std::string_view::npos) { ok = false; return false; }
                i = e + 2;
                return parseNode(out);
            }
            if (startsWith("<!--")) {
                auto e = s.find("-->", i);
                if (e == std::string_view::npos) { ok = false; return false; }
                i = e + 3;
                return parseNode(out);
            }
            ok = false;
            return false;
        }
        ++i; // '<'
        auto nameEnd = s.find_first_of(" \t\r\n/>", i);
        if (nameEnd == std::string_view::npos) { ok = false; return false; }
        out.name = std::string(s.substr(i, nameEnd - i));
        i = nameEnd;
        // attributs
        while (ok) {
            skipWs();
            if (startsWith("/>")) { i += 2; return true; }
            if (startsWith(">")) { ++i; break; }
            auto ae = s.find_first_of("= \t\r\n", i);
            if (ae == std::string_view::npos) { ok = false; return false; }
            XmlAttr a;
            a.name = std::string(s.substr(i, ae - i));
            i = ae;
            skipWs();
            if (i >= s.size() || s[i] != '=') { ok = false; return false; }
            ++i;
            skipWs();
            if (i >= s.size() || s[i] != '"') { ok = false; return false; }
            ++i;
            auto ve = s.find('"', i);
            if (ve == std::string_view::npos) { ok = false; return false; }
            a.value = decodeEntities(std::string(s.substr(i, ve - i)));
            i = ve + 1;
            out.attrs.push_back(std::move(a));
        }
        // contenu : texte + enfants jusqu'à </name>
        std::string text;
        while (ok && i < s.size()) {
            if (startsWith("</")) {
                auto e = s.find('>', i);
                if (e == std::string_view::npos) { ok = false; return false; }
                auto closeName = trim(s.substr(i + 2, e - i - 2));
                if (closeName != out.name) { ok = false; return false; }
                i = e + 1;
                out.text = decodeEntities(std::string(trim(text)));
                return true;
            }
            if (startsWith("<!--")) {
                auto e = s.find("-->", i);
                if (e == std::string_view::npos) { ok = false; return false; }
                i = e + 3;
                continue;
            }
            if (startsWith("<")) {
                XmlNode child;
                if (!parseNode(child)) { ok = false; return false; }
                out.children.push_back(std::move(child));
                continue;
            }
            text.push_back(s[i]);
            ++i;
        }
        ok = false;
        return false;
    }
};

const XmlAttr* findAttr(const XmlNode& n, std::string_view name) {
    for (const auto& a : n.attrs) if (a.name == name) return &a;
    return nullptr;
}

const XmlNode* findChild(const XmlNode& n, std::string_view name) {
    for (const auto& c : n.children) if (c.name == name) return &c;
    return nullptr;
}

bool isWidgetType(std::string_view t) {
    for (auto v : kWidgetTypes) if (t == v) return true;
    return false;
}

double attrDouble(const XmlAttr* a, double dflt = 0) {
    if (!a) return dflt;
    try { return std::stod(a->value); } catch (...) { return dflt; }
}

} // namespace

DashboardFile parseDashboardFile(std::string_view xml) {
    DashboardFile dash;
    Parser p{xml};
    XmlNode root;
    if (!p.parseNode(root) || !p.ok || root.name != "mxfile") {
        dash.error = "XML invalide : racine <mxfile> attendue";
        return dash;
    }
    if (const XmlAttr* v = findAttr(root, "dashboardVersion")) {
        try {
            dash.dashboardVersion = std::stoi(v->value);
        } catch (...) {
            dash.error = "dashboardVersion non entier";
            return dash;
        }
    }
    if (dash.dashboardVersion > kDashboardVersion ||
        dash.dashboardVersion < kDashboardVersion - 1) {
        dash.error = "dashboardVersion " + std::to_string(dash.dashboardVersion) +
                     " non supportée (attendu <= " + std::to_string(kDashboardVersion) + ")";
        return dash;
    }
    const XmlNode* diagram = findChild(root, "diagram");
    if (!diagram) { dash.error = "élément <diagram> absent"; return dash; }
    if (const XmlAttr* n = findAttr(*diagram, "name")) dash.name = n->value;
    const XmlNode* model = findChild(*diagram, "mxGraphModel");
    const XmlNode* rootCells = model ? findChild(*model, "root") : nullptr;
    if (!rootCells) { dash.error = "mxGraphModel/root absent"; return dash; }

    for (const auto& cell : rootCells->children) {
        if (cell.name != "mxCell") continue;
        const XmlAttr* userObject = findAttr(cell, "UserObject") != nullptr ? findAttr(cell, "UserObject") : nullptr;
        // Une cellule widget : UserObject présent (présence de l'attribut, peu importe la valeur)
        // ou attribut "type" non vide.
        const XmlAttr* typeAttr = findAttr(cell, "type");
        const XmlAttr* variableAttr = findAttr(cell, "variable");
        if (!typeAttr && !userObject) continue;
        Widget w;
        if (const XmlAttr* id = findAttr(cell, "id")) w.id = id->value;
        if (typeAttr) w.type = typeAttr->value;
        if (variableAttr) w.variable = variableAttr->value;
        if (const XmlAttr* label = findAttr(cell, "label")) w.label = label->value;
        if (const XmlNode* geo = findChild(cell, "mxGeometry")) {
            if (const XmlAttr* x = findAttr(*geo, "x")) w.x = attrDouble(x);
            if (const XmlAttr* y = findAttr(*geo, "y")) w.y = attrDouble(y);
            if (const XmlAttr* wd = findAttr(*geo, "width")) w.width = attrDouble(wd);
            if (const XmlAttr* ht = findAttr(*geo, "height")) w.height = attrDouble(ht);
        }
        if (!w.type.empty() && !isWidgetType(w.type)) {
            dash.error = "type de widget inconnu : " + w.type + " (widget " + w.id + ")";
            return dash;
        }
        dash.widgets.push_back(std::move(w));
    }
    dash.valid = true;
    return dash;
}

std::string serializeDashboardFile(const DashboardFile& dash) {
    std::ostringstream o;
    o << "<mxfile dashboardVersion=\"" << dash.dashboardVersion << "\" host=\"modelica_dashboard\">\n";
    o << "  <diagram name=\"" << dash.name << "\" id=\"dashboard\">\n";
    o << "    <mxGraphModel dx=\"800\" dy=\"600\" grid=\"1\" gridSize=\"10\" guides=\"1\" arrows=\"1\" fold=\"1\" page=\"1\" pageScale=\"1\" pageWidth=\"850\" pageHeight=\"1100\" math=\"0\" shadow=\"0\">\n";
    o << "      <root>\n";
    o << "        <mxCell id=\"0\"/>\n";
    o << "        <mxCell id=\"1\" parent=\"0\"/>\n";
    int id = 2;
    for (const auto& w : dash.widgets) {
        o << "        <mxCell id=\"" << (w.id.empty() ? std::to_string(id++) : w.id)
          << "\" type=\"" << w.type << "\" variable=\"" << w.variable << "\" label=\""
          << w.label << "\" UserObject=\"1\" vertex=\"1\" parent=\"1\">\n";
        o << "          <mxGeometry x=\"" << w.x << "\" y=\"" << w.y << "\" width=\"" << w.width
          << "\" height=\"" << w.height << "\" as=\"geometry\"/>\n";
        o << "        </mxCell>\n";
    }
    o << "      </root>\n    </mxGraphModel>\n  </diagram>\n</mxfile>\n";
    return o.str();
}

std::vector<std::string> unconnectedVariables(
    const std::vector<std::string>& modelVariables,
    const DashboardFile& dash) {
    std::vector<std::string> out;
    for (const auto& w : dash.widgets) {
        if (w.variable.empty()) continue;
        if (std::find(modelVariables.begin(), modelVariables.end(), w.variable) == modelVariables.end())
            out.push_back(w.variable);
    }
    return out;
}

} // namespace dashboard
