#include "viewer_core.hpp"

#include <fstream>
#include <sstream>

namespace dashboard {

ViewerCore::ViewerCore(std::unique_ptr<UiSink> sink) : sink_(std::move(sink)) {}

ViewerCore::~ViewerCore() { stop(); }

bool ViewerCore::start() {
    PivotEvents ev;
    ev.onTick = [this](const json& t) { sink_->send(t); };
    ev.onSimState = [this](std::string_view s) { sink_->send(makeSimState(s)); };
    ev.onUnconnected = [this](const std::vector<std::string>& v) { sink_->send(makeUnconnected(v)); };
    ev.onDashboard = [this](const json& d) { sink_->send(d); };
    ev.onData = [this](const json& d) { (void)d; };
    ev.onSimConnected = [this]() { sink_->send(json{{"type", "simEvent"}, {"connected", true}}); };
    ev.onSimDisconnected = [this]() { sink_->send(json{{"type", "simEvent"}, {"connected", false}}); };
    ev.onProtocolError = [this](const json& e) { sink_->send(e); };
    return pivot_.start(std::move(ev));
}

void ViewerCore::stop() { pivot_.stop(); }

void ViewerCore::onUiMessage(const json& msg) {
    if (!msg.is_object()) return;
    const std::string type = msg.value("type", "");
    if (type == "cmd") {
        pivot_.sendCommand(msg); // §8.5 viewer -> pivot -> sim
    } else if (type == "loadDashboard" && msg.contains("path")) {
        // mode manuel (D11) : unconnected calculé contre les variables du dernier handshake
        pivot_.loadDashboardFile(msg["path"].get<std::string>(), pivot_.modelVariables());
    } else if (type == "reload") {
        // D13 : relit le fichier courant entre deux runs (EF-E4)
        // (le chemin courant est conservé côté shell ; ici on ne fait que relayer)
    } else if (type == "openEditor") {
        // D30 : l'iframe draw.io est ouvert côté shell (WebView2) ; en headless, no-op loggé
        sink_->send(json{{"type", "log"}, {"text", "openEditor (headless: no-op)"}});
    }
}

} // namespace dashboard
