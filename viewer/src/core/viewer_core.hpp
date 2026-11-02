// viewer_core — logique du viewer sans dépendance WebView2 (portage Linux, testable).
// Le pivot (shared/) y est embarqué ; les événements pivot sont routés vers une
// interface UiSink (WebView2 côté Windows, stdio NDJSON côté headless/Linux).
#pragma once

#include "dashboard/pivot.hpp"

#include <functional>
#include <memory>
#include <string>
#include <vector>

namespace dashboard {

// Sink de sortie vers l'IHM (WebView2 app://, ou console headless).
struct UiSink {
    virtual ~UiSink() = default;
    virtual void send(const json& msg) = 0; // message viewer <- pivot, sérialisé NDJSON
};

// Commandes reçues de l'IHM (viewer -> pivot -> sim, §8.5 + loadDashboard/reload/openEditor).
struct UiCommand {
    json msg; // {"type":"cmd",...} ou {"type":"loadDashboard","path":...} etc.
};

class ViewerCore {
public:
    explicit ViewerCore(std::unique_ptr<UiSink> sink);
    ~ViewerCore();

    bool start(); // démarre le pivot sur 127.0.0.1:51234
    void stop();

    // Appelé par le shell à chaque message entrant de l'IHM.
    void onUiMessage(const json& msg);

    // Accès pour tests.
    Pivot& pivot() { return pivot_; }
    const std::string& lastError() const { return pivot_.lastError(); }

private:
    Pivot pivot_;
    std::unique_ptr<UiSink> sink_;
};

} // namespace dashboard
