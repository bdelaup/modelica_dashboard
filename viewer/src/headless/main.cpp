// Shell headless du viewer (Linux/macOS/CI) : viewer_core + UiSink stdio NDJSON.
// Protocole du shell (stdio, un JSON par ligne) :
//   entrée  (UI -> viewer) : {"type":"cmd",...} §8.5, {"type":"loadDashboard","path":...}, {"type":"reload"}
//   sortie  (viewer -> UI) : tick / simState / unconnected / dashboard / simEvent / error (protocole.md §7)
// Usage : viewer_headless [--dashboard fichier.drawio]
#include "viewer_core.hpp"

#include <cstdio>
#include <cstring>
#include <iostream>
#include <memory>
#include <string>

using namespace dashboard;

namespace {
struct StdioSink : UiSink {
    void send(const json& msg) override {
        std::string line = msg.dump();
        line.push_back('\n');
        fwrite(line.data(), 1, line.size(), stdout);
        fflush(stdout);
    }
};
} // namespace

int main(int argc, char** argv) {
    std::string dashboardPath;
    for (int i = 1; i < argc; ++i) {
        if (std::strcmp(argv[i], "--dashboard") == 0 && i + 1 < argc) dashboardPath = argv[++i];
    }
    ViewerCore core(std::make_unique<StdioSink>());
    if (!core.start()) {
        std::cerr << "pivot: port 51234 indisponible\n";
        return 1;
    }
    if (!dashboardPath.empty()) {
        core.onUiMessage(json{{"type", "loadDashboard"}, {"path", dashboardPath}});
    }
    std::string line;
    while (std::getline(std::cin, line)) {
        if (line.empty()) continue;
        auto msg = json::parse(line, nullptr, false);
        if (msg.is_discarded()) {
            std::cout << makeError(ErrorCode::BadJson, "ligne invalide").dump() << "\n";
            std::cout.flush();
            continue;
        }
        core.onUiMessage(msg);
    }
    core.stop();
    return 0;
}
