// Pivot — serveur loopback embarqué dans viewer.exe (D2/D26, spec §3).
// Portable : POSIX (tests/CI Linux) et Winsock (Windows) via une couche socket interne.
// Une seule connexion de simulation acceptée (D25) ; sinon error/busy puis fermeture.
#pragma once

#include "dashboard/protocol.hpp"

#include <atomic>
#include <functional>
#include <mutex>
#include <string>
#include <thread>
#include <vector>

namespace dashboard {

// Événements remontés au viewer (via le pont WebView2 en production, callbacks en test).
struct PivotEvents {
    // Reçu de la sim, validé.
    std::function<void(const json& data)> onData;                 // "data"
    std::function<void(std::string_view state)> onSimState;       // "state" -> simState
    // Émis vers le viewer.
    std::function<void(const json& tick)> onTick;                 // "tick"
    std::function<void(const std::vector<std::string>&)> onUnconnected;
    std::function<void(const json& dashboard)> onDashboard;       // source auto|manual
    // Cycle de vie.
    std::function<void()> onSimConnected;
    std::function<void()> onSimDisconnected;
    std::function<void(const json& error)> onProtocolError;
};

class Pivot {
public:
    Pivot();
    ~Pivot();
    Pivot(const Pivot&) = delete;
    Pivot& operator=(const Pivot&) = delete;

    // Ouvre 127.0.0.1:kPort en écoute et démarre le thread serveur. false si échec.
    bool start(PivotEvents events);
    void stop();
    bool isListening() const { return listening_.load(); }
    bool simConnected() const { return connected_.load(); }

    // Envoie une commande à la sim connectée (viewer -> pivot -> sim, §8.5).
    // Retourne false si aucune sim connectée ou envoi impossible.
    bool sendCommand(const json& cmd);

    // Mode manuel : charge un fichier dashboard, calcule les non-connectés, émet onDashboard.
    // Retourne false si le fichier est illisible/invalide (erreur dans lastError).
    bool loadDashboardFile(const std::string& path, const std::vector<std::string>& modelVariables);

    // Définit les variables du modèle courant (reçues au handshake) pour le calcul
    // des non-connectés en mode auto.
    void setModelVariables(std::vector<std::string> variables);

    const std::string& lastError() const { return lastError_; }

private:
    void serverLoop();
    void clientLoop();
    void handleMessage(const json& msg);
    bool sendJson(const json& msg);
    void setError(const json& e) { std::lock_guard<std::mutex> lk(errMutex_); lastError_ = e.dump(); if (events_.onProtocolError) events_.onProtocolError(e); }

    int listenFd_ = -1;
    int clientFd_ = -1;
    std::thread serverThread_;
    std::thread clientThread_;
    std::atomic<bool> running_{false};
    std::atomic<bool> listening_{false};
    std::atomic<bool> connected_{false};
    PivotEvents events_;
    std::mutex sendMutex_;
    std::mutex errMutex_;
    std::string lastError_;
    std::vector<std::string> modelVariables_;
    std::string pendingBuffer_;
    std::uint64_t lastSeq_ = 0;
    // ratio temps réel (EF-V5) : fenêtre glissante sur les 50 derniers pas
    double lastSimTime_ = 0.0;
    double lastRealTime_ = 0.0;
    double ratio_ = 1.0;
};

} // namespace dashboard
